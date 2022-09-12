// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/reboot_util.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromecast/public/reboot_shlib.h"

// This is a partial implementation of the reboot_util.h interface.
// The remaining methods are defined in other reboot_util_*.cc depending
// on which platform/product they are for.

namespace chromecast {
namespace {
RebootUtil::RebootCallback& GetTestRebootCallback() {
  static base::NoDestructor<RebootUtil::RebootCallback> callback;
  return *callback;
}
}  // namespace

// static
bool RebootUtil::IsRebootSupported() {
  return RebootShlib::IsSupported();
}

// static
bool RebootUtil::IsValidRebootSource(RebootShlib::RebootSource reboot_source) {
  switch (reboot_source) {
    case RebootShlib::RebootSource::UNKNOWN:
    case RebootShlib::RebootSource::FORCED:
    case RebootShlib::RebootSource::API:
    case RebootShlib::RebootSource::NIGHTLY:
    case RebootShlib::RebootSource::OTA:
    case RebootShlib::RebootSource::WATCHDOG:
    case RebootShlib::RebootSource::PROCESS_MANAGER:
    case RebootShlib::RebootSource::CRASH_UPLOADER:
    case RebootShlib::RebootSource::FDR:
    case RebootShlib::RebootSource::HW_WATCHDOG:
    case RebootShlib::RebootSource::SW_OTHER:
    case RebootShlib::RebootSource::OVERHEAT:
    case RebootShlib::RebootSource::REGENERATE_CLOUD_ID:
    case RebootShlib::RebootSource::REPEATED_OOM:
    case RebootShlib::RebootSource::UTILITY_PROCESS_CRASH:
    case RebootShlib::RebootSource::GRACEFUL_RESTART:
    case RebootShlib::RebootSource::UNGRACEFUL_RESTART:
    case RebootShlib::RebootSource::MULTI_SERVICE_BUG:
    case RebootShlib::RebootSource::POWER_MANAGER_SHUTDOWN:
    case RebootShlib::RebootSource::EXPERIMENT_CHANGE:
    case RebootShlib::RebootSource::ANOMALY:
    case RebootShlib::RebootSource::KERNEL_PANIC:
      return true;
    default:
      return false;
  }
}

// static
bool RebootUtil::IsRebootSourceSupported(
    RebootShlib::RebootSource reboot_source) {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsRebootSourceSupported(reboot_source);
}

// static
bool RebootUtil::RebootNow(RebootShlib::RebootSource reboot_source) {
  // If we have a testing callback avoid calling RebootShlib::RebootNow
  // because it will crash our test
  RebootUtil::RebootCallback& callback = GetTestRebootCallback();
  SetNextRebootSource(reboot_source);
  if (callback) {
    LOG(WARNING) << "Using reboot callback for test! Device will not reboot!";
    return callback.Run(reboot_source);
  }
  DCHECK(IsRebootSourceSupported(reboot_source));
  return RebootShlib::RebootNow(reboot_source);
}

// static
bool RebootUtil::IsFdrForNextRebootSupported() {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsFdrForNextRebootSupported();
}

// static
void RebootUtil::SetFdrForNextReboot() {
  DCHECK(IsFdrForNextRebootSupported());
  RebootShlib::SetFdrForNextReboot();
}

// static
bool RebootUtil::IsOtaForNextRebootSupported() {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsOtaForNextRebootSupported();
}

// static
void RebootUtil::SetOtaForNextReboot() {
  DCHECK(IsOtaForNextRebootSupported());
  RebootShlib::SetOtaForNextReboot();
}

// static
bool RebootUtil::IsClearOtaForNextRebootSupported() {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsClearOtaForNextRebootSupported();
}

// static
void RebootUtil::ClearOtaForNextReboot() {
  DCHECK(IsClearOtaForNextRebootSupported());
  RebootShlib::ClearOtaForNextReboot();
}

// static
void RebootUtil::SetRebootCallbackForTest(
    const RebootUtil::RebootCallback& callback) {
  GetTestRebootCallback() = callback;
}

// static
void RebootUtil::ClearRebootCallbackForTest() {
  GetTestRebootCallback().Reset();
}

}  // namespace chromecast
