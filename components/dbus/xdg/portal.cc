// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/portal.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/systemd.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

namespace {

constexpr char kVersionProperty[] = "version";

class PortalRegistrar {
 public:
  PortalRegistrar() = default;
  ~PortalRegistrar() = default;

  void Request(dbus::Bus* bus,
               std::string_view interface_name,
               PortalSetupCallback callback) {
    bus->AssertOnOriginThread();

    if (state_ == PortalRegistrarState::kFailed) {
      std::move(callback).Run(0);
      return;
    }

    if (state_ == PortalRegistrarState::kSuccess) {
      if (auto it = versions_.find(interface_name); it != versions_.end()) {
        std::move(callback).Run(it->second);
        return;
      }
      if (default_version_for_testing_) {
        std::move(callback).Run(*default_version_for_testing_);
        return;
      }
    }

    auto& callbacks = pending_callbacks_[std::string(interface_name)];
    const bool first_request_for_interface = callbacks.empty();
    callbacks.push_back(std::move(callback));

    switch (state_) {
      case PortalRegistrarState::kIdle:
        state_ = PortalRegistrarState::kInitializing;
        bus_ = bus;
        internal::SetSystemdScopeUnitNameForXdgPortal(
            bus, base::BindOnce(&PortalRegistrar::OnSystemdUnitNameSet,
                                weak_ptr_factory_.GetWeakPtr()));
        return;
      case PortalRegistrarState::kInitializing:
        CHECK_EQ(bus_.get(), bus);
        // The version is queried once setup completes.
        return;
      case PortalRegistrarState::kSuccess:
        // If this is not the first request, a version query for this interface
        // is already in flight.
        if (first_request_for_interface) {
          GetVersion(bus, std::string(interface_name));
        }
        return;
      case PortalRegistrarState::kFailed:
        NOTREACHED();
    }
  }

  void SetStateForTesting(PortalRegistrarState state) {
    state_ = state;
    bus_ = nullptr;
    versions_.clear();
    pending_callbacks_.clear();
    default_version_for_testing_ =
        state == PortalRegistrarState::kSuccess
            ? std::optional<uint32_t>(kDefaultPortalVersionForTesting)
            : std::nullopt;
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void SetInterfaceVersionForTesting(std::string_view interface_name,
                                     uint32_t version) {
    versions_[std::string(interface_name)] = version;
  }

 private:
  void OnSystemdUnitNameSet(internal::SystemdUnitStatus status) {
    systemd_unit_status_ = status;
    dbus_utils::CheckForServiceAndStart(
        bus_.get(), kPortalServiceName,
        base::BindOnce(&PortalRegistrar::OnServiceChecked,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnServiceChecked(std::optional<bool> service_started) {
    if (!service_started.value_or(false)) {
      OnSetupFailed();
      return;
    }

    // If running under Flatpak or Snap, the app id is determined by the
    // sandbox, so there's no need to register.
    if (systemd_unit_status_ ==
        internal::SystemdUnitStatus::kUnitNotNecessary) {
      OnSetupSucceeded();
      return;
    }

    // Even when a systemd scope was started, register explicitly:
    // xdg-desktop-portal >= 1.21 requires a registered app id for some
    // interfaces (e.g. GlobalShortcuts).
    Register();

    // Listen for NameOwnerChanged to re-register if needed.
    dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
        kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
    proxy->SetNameOwnerChangedCallback(base::BindRepeating(
        &PortalRegistrar::OnNameOwnerChanged, weak_ptr_factory_.GetWeakPtr()));
  }

  void Register() {
    auto env = base::Environment::Create();
    std::string app_name = version_info::nix::GetAppName(*env);

    dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
        kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

    std::map<std::string, dbus_utils::Variant> options;

    dbus_utils::CallMethod<"sa{sv}", "">(
        proxy, kRegistryInterface, kMethodRegister,
        base::BindOnce(&PortalRegistrar::OnRegisterResponse,
                       weak_ptr_factory_.GetWeakPtr()),
        app_name, options);
  }

  void OnRegisterResponse(dbus_utils::CallMethodResult<> result) {
    if (!result.has_value()) {
      // Failing to register is not an error as long as the portal is available.
      LOG(WARNING) << "Failed to register with " << kRegistryInterface;
    }

    // Re-registration after a portal restart also lands here, after setup has
    // already succeeded.
    if (state_ == PortalRegistrarState::kInitializing) {
      OnSetupSucceeded();
    }
  }

  void OnSetupSucceeded() {
    state_ = PortalRegistrarState::kSuccess;
    // Copy the interface names first, since replies may modify
    // `pending_callbacks_`.
    std::vector<std::string> interface_names;
    for (const auto& entry : pending_callbacks_) {
      interface_names.push_back(entry.first);
    }
    for (const std::string& interface_name : interface_names) {
      GetVersion(bus_.get(), interface_name);
    }
  }

  void OnSetupFailed() {
    state_ = PortalRegistrarState::kFailed;
    auto pending_callbacks = std::move(pending_callbacks_);
    pending_callbacks_.clear();
    for (auto& [interface_name, callbacks] : pending_callbacks) {
      for (auto& callback : callbacks) {
        std::move(callback).Run(0);
      }
    }
  }

  void GetVersion(dbus::Bus* bus, const std::string& interface_name) {
    dbus::ObjectProxy* proxy = bus->GetObjectProxy(
        kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

    dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, "Get");
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(interface_name);
    writer.AppendString(kVersionProperty);
    proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PortalRegistrar::OnGetVersionReply,
                       weak_ptr_factory_.GetWeakPtr(), interface_name));
  }

  void OnGetVersionReply(const std::string& interface_name,
                         dbus::Response* response) {
    uint32_t version = 0;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopVariantOfUint32(&version)) {
        version = 0;
      }
    }
    versions_[interface_name] = version;

    auto node = pending_callbacks_.extract(interface_name);
    if (node.empty()) {
      return;
    }
    for (auto& callback : node.mapped()) {
      std::move(callback).Run(version);
    }
  }

  void OnNameOwnerChanged(const std::string& old_owner,
                          const std::string& new_owner) {
    // The set of available interfaces may change when the portal restarts,
    // so re-query versions on the next request.
    versions_.clear();
    if (!new_owner.empty()) {
      // Service restarted or appeared. Re-register.
      Register();
    }
  }

  scoped_refptr<dbus::Bus> bus_;
  PortalRegistrarState state_ = PortalRegistrarState::kIdle;
  std::optional<internal::SystemdUnitStatus> systemd_unit_status_;
  // Cached interface versions. 0 means the interface is unavailable.
  std::map<std::string, uint32_t, std::less<>> versions_;
  // Callbacks waiting for setup to complete or for an interface version query.
  std::map<std::string, std::vector<PortalSetupCallback>, std::less<>>
      pending_callbacks_;
  std::optional<uint32_t> default_version_for_testing_;
  base::WeakPtrFactory<PortalRegistrar> weak_ptr_factory_{this};
};

}  // namespace

PortalRegistrar* GetPortalRegistrar() {
  static base::NoDestructor<PortalRegistrar> registrar;
  return registrar.get();
}

void RequestXdgDesktopPortal(dbus::Bus* bus,
                             std::string_view interface_name,
                             PortalSetupCallback callback) {
  GetPortalRegistrar()->Request(bus, interface_name, std::move(callback));
}

void SetPortalStateForTesting(PortalRegistrarState state) {
  GetPortalRegistrar()->SetStateForTesting(state);  // IN-TEST
}

void SetPortalInterfaceVersionForTesting(std::string_view interface_name,
                                         uint32_t version) {
  GetPortalRegistrar()->SetInterfaceVersionForTesting(  // IN-TEST
      interface_name, version);
}

}  // namespace dbus_xdg
