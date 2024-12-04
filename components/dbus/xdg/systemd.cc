// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include <vector>

#include "base/environment.h"
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
#include "dbus/object_proxy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace dbus_xdg {

// Systemd dictionaries use structs instead of dict entries for some reason.
template <typename T>
using Dict = DbusArray<DbusStruct<DbusString, T>>;
using VarDict = Dict<DbusVariant>;

using SystemdUnitCallbacks = std::vector<SystemdUnitCallback>;
using StatusOrCallbacks =
    absl::variant<SystemdUnitStatus, SystemdUnitCallbacks>;

namespace {

constexpr char kServiceNameSystemd[] = "org.freedesktop.systemd1";
constexpr char kObjectPathSystemd[] = "/org/freedesktop/systemd1";
constexpr char kInterfaceSystemdManager[] = "org.freedesktop.systemd1.Manager";
constexpr char kMethodStartTransientUnit[] = "StartTransientUnit";

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

// Global state for cached result or pending callbacks.
StatusOrCallbacks& GetUnitNameState() {
  static base::NoDestructor<StatusOrCallbacks> state(
      std::in_place_type<SystemdUnitCallbacks>);
  return *state;
}

void SetStateAndRunCallbacks(SystemdUnitStatus result) {
  auto& state = GetUnitNameState();
  auto callbacks = std::move(absl::get<SystemdUnitCallbacks>(state));
  state = result;
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}

void OnStartTransientUnitResponse(dbus::Response* response) {
  SystemdUnitStatus result = response ? SystemdUnitStatus::kUnitStarted
                                      : SystemdUnitStatus::kFailedToStart;
  SetStateAndRunCallbacks(result);
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
  std::string channel;
  env->GetVar(kChannelEnvVar, &channel);
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
  systemd->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                      base::BindOnce(&OnStartTransientUnitResponse));
}

}  // namespace

void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         SystemdUnitCallback callback) {
#if DCHECK_IS_ON()
  static base::SequenceChecker sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
#endif

  auto& state = GetUnitNameState();

  if (absl::holds_alternative<SystemdUnitStatus>(state)) {
    // If the result is already cached, run the callback immediately.
    std::move(callback).Run(absl::get<SystemdUnitStatus>(state));
    return;
  }

  // Add the callback to the list of pending callbacks.
  auto& callbacks = absl::get<SystemdUnitCallbacks>(state);
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
