// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include "base/environment.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "components/dbus/properties/types.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

// Systemd dictionaries use structs instead of dict entries for some reason.
template <typename T>
using Dict = DbusArray<DbusStruct<DbusString, T>>;
using VarDict = Dict<DbusVariant>;

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

void OnStartTransientUnitResponse(SystemdUnitCallback callback,
                                  dbus::Response* response) {
  std::move(callback).Run(response ? SystemdUnitStatus::kUnitStarted
                                   : SystemdUnitStatus::kFailedToStart);
}

}  // namespace

void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         SystemdUnitCallback callback) {
  auto env = base::Environment::Create();
  if (env->HasVar("FLATPAK_SANDBOX_DIR") || env->HasVar(("SNAP"))) {
    // xdg-desktop-portal has a separate reliable way of detecting the
    // application name for Flatpak and Snap environments, so the systemd unit
    // is not necessary in these cases.
    std::move(callback).Run(SystemdUnitStatus::kUnitNotNecessary);
    return;
  }

  pid_t pid = getpid();
  if (pid <= 1) {
    std::move(callback).Run(SystemdUnitStatus::kInvalidPid);
    return;
  }

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
  systemd->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(OnStartTransientUnitResponse, std::move(callback)));
}

}  // namespace dbus_xdg
