// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include <string>
#include <variant>
#include <vector>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/utils/name_has_owner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace dbus_xdg {

// Systemd dictionaries use structs instead of dict entries for some reason.
template <typename T>
using Dict = DbusArray<DbusStruct<DbusString, T>>;
using VarDict = Dict<DbusVariant>;

using SystemdUnitCallbacks = std::vector<SystemdUnitCallback>;
using StatusOrCallbacks = std::variant<SystemdUnitStatus, SystemdUnitCallbacks>;

namespace {

constexpr char kServiceNameSystemd[] = "org.freedesktop.systemd1";
constexpr char kObjectPathSystemd[] = "/org/freedesktop/systemd1";
constexpr char kInterfaceSystemdManager[] = "org.freedesktop.systemd1.Manager";
constexpr char kMethodStartTransientUnit[] = "StartTransientUnit";
constexpr char kMethodGetUnit[] = "GetUnit";

constexpr char kInterfaceSystemdUnit[] = "org.freedesktop.systemd1.Unit";
constexpr char kActiveStateProp[] = "ActiveState";

constexpr char kUnitNameFormat[] = "app-$1$2-$3.scope";

constexpr char kModeReplace[] = "replace";

constexpr char kChannelEnvVar[] = "CHROME_VERSION_EXTRA";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kAppNamePrefix[] = "com.google.Chrome";
#else
constexpr char kAppNamePrefix[] = "org.chromium.Chromium";
#endif

const char* GetAppNameSuffix(const std::string& channel) {
  if (channel == "beta") {
    return ".beta";
  }
  if (channel == "unstable") {
    return ".unstable";
  }
  if (channel == "canary") {
    return ".canary";
  }
  // No suffix for stable. Also if the channel is unknown, the most likely
  // scenario is the user is running the binary directly and not getting the
  // environment variable set, so assume stable to minimize potential risk of
  // settings or data loss.
  return "";
}

// Declare this helper for SystemdUnitActiveStateWatcher to be used.
void SetStateAndRunCallbacks(SystemdUnitStatus result);

// Watches the object to become active and fires callbacks via
// SetStateAndRunCallbacks. The callbacks are fired whenever a response with the
// state being "active" or "failed" (or similar) comes.
//
// PS firing callbacks results in destroying this object. So any references
// to this become invalid.
class SystemdUnitActiveStateWatcher : public dbus::PropertySet {
 public:
  SystemdUnitActiveStateWatcher(scoped_refptr<dbus::Bus> bus,
                                dbus::ObjectProxy* object_proxy)
      : dbus::PropertySet(object_proxy,
                          kInterfaceSystemdUnit,
                          base::BindRepeating(
                              &SystemdUnitActiveStateWatcher::OnPropertyChanged,
                              base::Unretained(this))),
        bus_(bus) {
    RegisterProperty(kActiveStateProp, &active_state_);
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
    if (callbacks_called_ || state_value == "activating" ||
        state_value == "reloading") {
      // Ignore if callbacks have already been fired or continue waiting until
      // the state changes to something else.
      return;
    }

    // There are other states as failed, inactive, and deactivating. Treat all
    // of them as failed.
    callbacks_called_ = true;
    SetStateAndRunCallbacks(state_value == "active"
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
    if (!keep_alive_ && callbacks_called_) {
      delete this;
    }
  }

  // Registered property that this listens updates to.
  dbus::Property<std::string> active_state_;

  // Indicates whether callbacks for the unit's state have been called.
  bool callbacks_called_ = false;

  // Control variable that helps to defer the destruction of |this| as deleting
  // self when the state changes to active during |OnGetAll| will result in a
  // segfault.
  bool keep_alive_ = true;

  scoped_refptr<dbus::Bus> bus_;
};

// Global state for cached result or pending callbacks.
StatusOrCallbacks& GetUnitNameState() {
  static base::NoDestructor<StatusOrCallbacks> state(
      std::in_place_type<SystemdUnitCallbacks>);
  return *state;
}

void SetStateAndRunCallbacks(SystemdUnitStatus result) {
  auto& state = GetUnitNameState();
  auto callbacks = std::move(std::get<SystemdUnitCallbacks>(state));
  state = result;
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}

void OnGetPathResponse(scoped_refptr<dbus::Bus> bus, dbus::Response* response) {
  dbus::MessageReader reader(response);
  dbus::ObjectPath obj_path;
  if (!response || !reader.PopObjectPath(&obj_path) || !obj_path.IsValid()) {
    // We didn't get a valid response. Treat this as failed service.
    SetStateAndRunCallbacks(SystemdUnitStatus::kFailedToStart);
    return;
  }

  dbus::ObjectProxy* unit_proxy =
      bus->GetObjectProxy(kServiceNameSystemd, obj_path);
  // Create the active state property watcher. It will destroy itself once
  // it gets notified about the state change.
  std::unique_ptr<SystemdUnitActiveStateWatcher> active_state_watcher =
      std::make_unique<SystemdUnitActiveStateWatcher>(bus, unit_proxy);
  active_state_watcher.release();
}

void WaitUnitActivateAndRunCallbacks(scoped_refptr<dbus::Bus> bus,
                                     std::string unit_name) {
  // Get the path of the unit, which looks similar to
  // /org/freedesktop/systemd1/unit/app_2dorg_2echromium_2eChromium_2d3182191_2escope
  // and then wait for it activation.
  dbus::ObjectProxy* systemd = bus->GetObjectProxy(
      kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  dbus::MethodCall method_call(kInterfaceSystemdManager, kMethodGetUnit);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name);

  systemd->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                      base::BindOnce(&OnGetPathResponse, std::move(bus)));
}

void OnStartTransientUnitResponse(scoped_refptr<dbus::Bus> bus,
                                  std::string unit_name,
                                  dbus::Response* response) {
  SystemdUnitStatus result = response ? SystemdUnitStatus::kUnitStarted
                                      : SystemdUnitStatus::kFailedToStart;
  // If the start of the unit failed, immediately notify the client. Otherwise,
  // wait for its activation.
  if (result == SystemdUnitStatus::kFailedToStart) {
    SetStateAndRunCallbacks(result);
  } else {
    WaitUnitActivateAndRunCallbacks(std::move(bus), unit_name);
  }
}

void OnNameHasOwnerResponse(scoped_refptr<dbus::Bus> bus,
                            std::optional<bool> name_has_owner) {
  if (!name_has_owner.value_or(false)) {
    SetStateAndRunCallbacks(SystemdUnitStatus::kNoSystemdService);
    return;
  }

  pid_t pid = getpid();
  if (pid <= 1) {
    SetStateAndRunCallbacks(SystemdUnitStatus::kInvalidPid);
    return;
  }

  auto env = base::Environment::Create();
  std::string channel = env->GetVar(kChannelEnvVar).value_or("");
  const char* app_name_suffix = GetAppNameSuffix(channel);

  // The unit naming format is specified in
  // https://systemd.io/DESKTOP_ENVIRONMENTS/
  auto unit_name = base::ReplaceStringPlaceholders(
      kUnitNameFormat,
      {kAppNamePrefix, app_name_suffix, base::NumberToString(pid)}, nullptr);

  auto* systemd = bus->GetObjectProxy(kServiceNameSystemd,
                                      dbus::ObjectPath(kObjectPathSystemd));
  dbus::MethodCall method_call(kInterfaceSystemdManager,
                               kMethodStartTransientUnit);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name);
  writer.AppendString(kModeReplace);
  // For now, only add this process to the new scope. It's possible to add all
  // PIDs in the process tree, but there's currently not a benefit.
  auto pids = MakeDbusArray(DbusUint32(pid));
  VarDict properties(
      MakeDbusStruct(DbusString("PIDs"), MakeDbusVariant(std::move(pids))));
  properties.Write(&writer);
  // No auxiliary units.
  Dict<VarDict>().Write(&writer);
  systemd->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnStartTransientUnitResponse, std::move(bus), unit_name));
}

}  // namespace

void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         SystemdUnitCallback callback) {
#if DCHECK_IS_ON()
  static base::SequenceChecker sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
#endif

  auto& state = GetUnitNameState();

  if (std::holds_alternative<SystemdUnitStatus>(state)) {
    // If the result is already cached, run the callback immediately.
    std::move(callback).Run(std::get<SystemdUnitStatus>(state));
    return;
  }

  // Add the callback to the list of pending callbacks.
  auto& callbacks = std::get<SystemdUnitCallbacks>(state);
  callbacks.push_back(std::move(callback));

  if (callbacks.size() > 1) {
    // An operation is already in progress.
    return;
  }

  auto env = base::Environment::Create();
  if (env->HasVar("FLATPAK_SANDBOX_DIR") || env->HasVar("SNAP")) {
    // xdg-desktop-portal has a separate reliable way of detecting the
    // application name for Flatpak and Snap environments, so the systemd unit
    // is not necessary in these cases.
    SetStateAndRunCallbacks(SystemdUnitStatus::kUnitNotNecessary);
    return;
  }

  // Check if the systemd service is available
  dbus_utils::NameHasOwner(
      bus, kServiceNameSystemd,
      base::BindOnce(&OnNameHasOwnerResponse, base::WrapRefCounted(bus)));
}

void ResetCachedStateForTesting() {
  GetUnitNameState() = SystemdUnitCallbacks();
}

}  // namespace dbus_xdg
