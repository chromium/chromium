// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/portal.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
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

class PortalRegistrar {
 public:
  PortalRegistrar() = default;
  ~PortalRegistrar() = default;

  void Request(dbus::Bus* bus, PortalSetupCallback callback) {
    bus->AssertOnOriginThread();

    // If initialization is already done, run the callback immediately,
    // otherwise add it to the callback list.
    if (state_ == PortalRegistrarState::kSuccess ||
        state_ == PortalRegistrarState::kFailed) {
      std::move(callback).Run(state_ == PortalRegistrarState::kSuccess);
      return;
    }
    callbacks_.push_back(std::move(callback));

    if (state_ == PortalRegistrarState::kInitializing) {
      CHECK_EQ(bus_.get(), bus);
      return;
    }

    CHECK_EQ(state_, PortalRegistrarState::kIdle);
    state_ = PortalRegistrarState::kInitializing;
    bus_ = bus;

    internal::SetSystemdScopeUnitNameForXdgPortal(
        bus, base::BindOnce(&PortalRegistrar::OnSystemdUnitNameSet,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void SetStateForTesting(PortalRegistrarState state) {
    state_ = state;
    bus_ = nullptr;
    callbacks_.clear();
    weak_ptr_factory_.InvalidateWeakPtrs();
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
      SetStateAndRunCallbacks(PortalRegistrarState::kFailed);
      return;
    }

    // If running under Flatpak or Snap, or unit started successfully, then no
    // need to register.
    if (systemd_unit_status_ ==
            internal::SystemdUnitStatus::kUnitNotNecessary ||
        systemd_unit_status_ == internal::SystemdUnitStatus::kUnitStarted) {
      SetStateAndRunCallbacks(PortalRegistrarState::kSuccess);
      return;
    }

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

    SetStateAndRunCallbacks(PortalRegistrarState::kSuccess);
  }

  void OnNameOwnerChanged(const std::string& old_owner,
                          const std::string& new_owner) {
    if (!new_owner.empty()) {
      // Service restarted or appeared. Re-register.
      Register();
    }
  }

  void SetStateAndRunCallbacks(PortalRegistrarState state) {
    state_ = state;
    bool success = (state_ == PortalRegistrarState::kSuccess);
    std::vector<PortalSetupCallback> callbacks;
    callbacks.swap(callbacks_);
    for (auto& callback : callbacks) {
      std::move(callback).Run(success);
    }
  }

  scoped_refptr<dbus::Bus> bus_;
  PortalRegistrarState state_ = PortalRegistrarState::kIdle;
  std::optional<internal::SystemdUnitStatus> systemd_unit_status_;
  std::vector<PortalSetupCallback> callbacks_;
  base::WeakPtrFactory<PortalRegistrar> weak_ptr_factory_{this};
};

}  // namespace

PortalRegistrar* GetPortalRegistrar() {
  static base::NoDestructor<PortalRegistrar> registrar;
  return registrar.get();
}

void RequestXdgDesktopPortal(dbus::Bus* bus, PortalSetupCallback callback) {
  GetPortalRegistrar()->Request(bus, std::move(callback));
}

void SetPortalStateForTesting(PortalRegistrarState state) {
  GetPortalRegistrar()->SetStateForTesting(state);  // IN-TEST
}

}  // namespace dbus_xdg
