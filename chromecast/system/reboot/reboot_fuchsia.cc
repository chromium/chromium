// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/feedback/cpp/fidl.h>
#include <fuchsia/hardware/power/statecontrol/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <zircon/types.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/no_destructor.h"
#include "chromecast/public/reboot_shlib.h"
#include "chromecast/system/reboot/reboot_util.h"

using fuchsia::feedback::LastReboot;
using fuchsia::feedback::LastRebootInfoProviderSyncPtr;
using fuchsia::feedback::RebootReason;
using fuchsia::hardware::power::statecontrol::Admin_Suspend_Result;
using fuchsia::hardware::power::statecontrol::AdminSyncPtr;
using fuchsia::hardware::power::statecontrol::SystemPowerState;

namespace chromecast {

AdminSyncPtr& GetAdminSyncPtr() {
  static base::NoDestructor<AdminSyncPtr> g_admin;
  return *g_admin;
}

LastRebootInfoProviderSyncPtr& GetLastRebootInfoProviderSyncPtr() {
  static base::NoDestructor<LastRebootInfoProviderSyncPtr> g_last_reboot_info;
  return *g_last_reboot_info;
}

void InitializeRebootShlib(const std::vector<std::string>& argv,
                           sys::ServiceDirectory* incoming_directory) {
  incoming_directory->Connect(GetAdminSyncPtr().NewRequest());
  incoming_directory->Connect(GetLastRebootInfoProviderSyncPtr().NewRequest());
}

// RebootShlib implementation:

// static
void RebootShlib::Initialize(const std::vector<std::string>& argv) {
  InitializeRebootShlib(
      argv, base::fuchsia::ComponentContextForCurrentProcess()->svc().get());
}

// static
void RebootShlib::Finalize() {}

// static
bool RebootShlib::IsSupported() {
  return true;
}

// static
bool RebootShlib::IsRebootSourceSupported(
    RebootShlib::RebootSource /* reboot_source */) {
  return true;
}

// static
bool RebootShlib::RebootNow(RebootSource reboot_source) {
  Admin_Suspend_Result out_result;
  zx_status_t status =
      GetAdminSyncPtr()->Suspend(SystemPowerState::REBOOT, &out_result);
  ZX_CHECK(status == ZX_OK, status) << "Failed to suspend device";
  return !out_result.is_err();
}

// static
bool RebootShlib::IsFdrForNextRebootSupported() {
  return false;
}

// static
void RebootShlib::SetFdrForNextReboot() {}

// static
bool RebootShlib::IsOtaForNextRebootSupported() {
  return false;
}

// static
void RebootShlib::SetOtaForNextReboot() {}

// RebootUtil implementation:

// static
void RebootUtil::Initialize(const std::vector<std::string>& argv) {
  RebootShlib::Initialize(argv);
}

// static
void RebootUtil::Finalize() {
  RebootShlib::Finalize();
}

// static
RebootShlib::RebootSource RebootUtil::GetLastRebootSource() {
  LastReboot last_reboot;
  zx_status_t status = GetLastRebootInfoProviderSyncPtr()->Get(&last_reboot);
  if (status != ZX_OK || last_reboot.IsEmpty() || !last_reboot.has_graceful()) {
    ZX_LOG(ERROR, status) << "Failed to get last reboot reason";
    return RebootShlib::RebootSource::UNKNOWN;
  }

  if (!last_reboot.has_reason()) {
    return last_reboot.graceful() ? RebootShlib::RebootSource::SW_OTHER
                                  : RebootShlib::RebootSource::FORCED;
  }

  switch (last_reboot.reason()) {
    case RebootReason::COLD:
    case RebootReason::BRIEF_POWER_LOSS:
    case RebootReason::BROWNOUT:
    case RebootReason::KERNEL_PANIC:
      return RebootShlib::RebootSource::FORCED;
    case RebootReason::SYSTEM_OUT_OF_MEMORY:
      return RebootShlib::RebootSource::REPEATED_OOM;
    case RebootReason::HARDWARE_WATCHDOG_TIMEOUT:
      return RebootShlib::RebootSource::HW_WATCHDOG;
    case RebootReason::SOFTWARE_WATCHDOG_TIMEOUT:
      return RebootShlib::RebootSource::WATCHDOG;
    default:
      return last_reboot.graceful() ? RebootShlib::RebootSource::SW_OTHER
                                    : RebootShlib::RebootSource::FORCED;
  }
}

// static
bool RebootUtil::SetNextRebootSource(
    RebootShlib::RebootSource /* reboot_source */) {
  return false;
}

}  // namespace chromecast
