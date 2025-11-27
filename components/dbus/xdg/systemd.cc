// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "build/branding_buildflags.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/systemd_constants.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace dbus_xdg {

// Systemd dictionaries use structs instead of dict entries for some reason.
template <typename T>
using Dict = std::vector<std::tuple<std::string, T>>;
using VarDict = Dict<dbus_utils::Variant>;

using internal::SystemdUnitCallback;
using internal::SystemdUnitStatus;
using SystemdUnitCallbacks = std::vector<SystemdUnitCallback>;

namespace {

constexpr char kUnitNameFormat[] = "app-$1-$2.scope";

constexpr char kModeReplace[] = "replace";

// Watches the object to become active and fires callbacks.
// The callbacks are fired whenever a response with the
// state being "active" or "failed" (or similar) comes.
//
// PS firing callbacks results in destroying this object. So any references
// to this become invalid.
class SystemdUnitActiveStateWatcher : public dbus::PropertySet {
 public:
  SystemdUnitActiveStateWatcher(scoped_refptr<dbus::Bus> bus,
                                dbus::ObjectProxy* object_proxy,
                                SystemdUnitCallback callback)
      : dbus::PropertySet(object_proxy,
                          kInterfaceSystemdUnit,
                          base::BindRepeating(
                              &SystemdUnitActiveStateWatcher::OnPropertyChanged,
                              base::Unretained(this))),
        bus_(bus),
        callback_(std::move(callback)) {
    RegisterProperty(kSystemdActiveStateProp, &active_state_);
    ConnectSignals();
    GetAll();
  }

  ~SystemdUnitActiveStateWatcher() override {
    bus_->RemoveObjectProxy(kServiceNameSystemd, object_proxy()->object_path(),
                            base::DoNothing());
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    DCHECK(active_state_.is_valid());
    const std::string state_value = active_state_.value();
    if (!callback_ || state_value == "activating" ||
        state_value == "reloading") {
      // Ignore if callbacks have already been fired or continue waiting until
      // the state changes to something else.
      return;
    }

    // There are other states as failed, inactive, and deactivating. Treat all
    // of them as failed.
    std::move(callback_).Run(state_value == kSystemdStateActive
                                 ? SystemdUnitStatus::kUnitStarted
                                 : SystemdUnitStatus::kFailedToStart);
    MaybeDeleteSelf();
  }

  void OnGetAll(dbus::Response* response) override {
    dbus::PropertySet::OnGetAll(response);
    keep_alive_ = false;
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!keep_alive_ && !callback_) {
      delete this;
    }
  }

  // Registered property that this listens updates to.
  dbus::Property<std::string> active_state_;

  // Control variable that helps to defer the destruction of |this| as deleting
  // self when the state changes to active during |OnGetAll| will result in a
  // segfault.
  bool keep_alive_ = true;

  scoped_refptr<dbus::Bus> bus_;
  SystemdUnitCallback callback_;
};

void OnGetPathResponse(scoped_refptr<dbus::Bus> bus,
                       SystemdUnitCallback callback,
                       dbus::Response* response) {
  dbus::MessageReader reader(response);
  dbus::ObjectPath obj_path;
  if (!response || !reader.PopObjectPath(&obj_path) || !obj_path.IsValid()) {
    // We didn't get a valid response. Treat this as failed service.
    std::move(callback).Run(SystemdUnitStatus::kFailedToStart);
    return;
  }

  dbus::ObjectProxy* unit_proxy =
      bus->GetObjectProxy(kServiceNameSystemd, obj_path);
  // Create the active state property watcher. It will destroy itself once
  // it gets notified about the state change.
  std::unique_ptr<SystemdUnitActiveStateWatcher> active_state_watcher =
      std::make_unique<SystemdUnitActiveStateWatcher>(bus, unit_proxy,
                                                      std::move(callback));
  active_state_watcher.release();
}

void WaitUnitActivateAndRunCallbacks(scoped_refptr<dbus::Bus> bus,
                                     std::string unit_name,
                                     SystemdUnitCallback callback) {
  // Get the path of the unit, which looks similar to
  // /org/freedesktop/systemd1/unit/app_2dorg_2echromium_2eChromium_2d3182191_2escope
  // and then wait for it activation.
  dbus::ObjectProxy* systemd = bus->GetObjectProxy(
      kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  dbus::MethodCall method_call(kInterfaceSystemdManager, kMethodGetUnit);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name);

  systemd->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnGetPathResponse, std::move(bus), std::move(callback)));
}

void OnStartTransientUnitResponse(scoped_refptr<dbus::Bus> bus,
                                  std::string unit_name,
                                  SystemdUnitCallback callback,
                                  dbus::Response* response) {
  SystemdUnitStatus result = response ? SystemdUnitStatus::kUnitStarted
                                      : SystemdUnitStatus::kFailedToStart;
  // If the start of the unit failed, immediately notify the client. Otherwise,
  // wait for its activation.
  if (result == SystemdUnitStatus::kFailedToStart) {
    std::move(callback).Run(result);
  } else {
    WaitUnitActivateAndRunCallbacks(std::move(bus), unit_name,
                                    std::move(callback));
  }
}

void OnNameHasOwnerResponse(scoped_refptr<dbus::Bus> bus,
                            SystemdUnitCallback callback,
                            std::optional<bool> name_has_owner) {
  if (!name_has_owner.value_or(false)) {
    std::move(callback).Run(SystemdUnitStatus::kNoSystemdService);
    return;
  }

  pid_t pid = getpid();
  if (pid <= 1) {
    std::move(callback).Run(SystemdUnitStatus::kInvalidPid);
    return;
  }

  std::string app_name =
      version_info::nix::GetAppName(*base::Environment::Create());

  // The unit naming format is specified in
  // https://systemd.io/DESKTOP_ENVIRONMENTS/
  auto unit_name = base::ReplaceStringPlaceholders(
      kUnitNameFormat, {app_name, base::NumberToString(pid)}, nullptr);

  auto* systemd = bus->GetObjectProxy(kServiceNameSystemd,
                                      dbus::ObjectPath(kObjectPathSystemd));
  dbus::MethodCall method_call(kInterfaceSystemdManager,
                               kMethodStartTransientUnit);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name);
  writer.AppendString(kModeReplace);
  // For now, only add this process to the new scope. It's possible to add all
  // PIDs in the process tree, but there's currently not a benefit.
  std::vector<uint32_t> pids = {static_cast<uint32_t>(pid)};
  VarDict properties;
  properties.emplace_back("PIDs",
                          dbus_utils::Variant::Wrap<"au">(std::move(pids)));
  dbus_utils::WriteValue(writer, properties);
  // No auxiliary units.
  dbus_utils::WriteValue(writer, Dict<VarDict>());
  systemd->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnStartTransientUnitResponse, std::move(bus), unit_name,
                     std::move(callback)));
}

}  // namespace

namespace internal {

void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         SystemdUnitCallback callback) {
#if DCHECK_IS_ON()
  static base::SequenceChecker sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
#endif

  auto env = base::Environment::Create();
  if (env->HasVar("FLATPAK_SANDBOX_DIR") || env->HasVar("SNAP")) {
    // xdg-desktop-portal has a separate reliable way of detecting the
    // application name for Flatpak and Snap environments, so the systemd unit
    // is not necessary in these cases.
    std::move(callback).Run(SystemdUnitStatus::kUnitNotNecessary);
    return;
  }

  // Check if the systemd service is available
  dbus_utils::NameHasOwner(
      bus, kServiceNameSystemd,
      base::BindOnce(&OnNameHasOwnerResponse, base::WrapRefCounted(bus),
                     std::move(callback)));
}

}  // namespace internal

}  // namespace dbus_xdg
