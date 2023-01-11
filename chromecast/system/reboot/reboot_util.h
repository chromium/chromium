// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SYSTEM_REBOOT_REBOOT_UTIL_H_
#define CHROMECAST_SYSTEM_REBOOT_REBOOT_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromecast/public/reboot_shlib.h"

namespace chromecast {

// A wrapper util for the RebootShlib interface. This acts as a translation
// layer between cast_shell call sites (such as the setup API, process_manager,
// and crash_uploader) and the actual RebootShlib API.
// The RebootShlib interface should never need to be used directly; instead
// prefer to use the RebootUtil interface.
class RebootUtil {
 public:
  static void Initialize(const std::vector<std::string>& argv);
  static void Finalize();

  // Indicates if any RebootSources are supported for rebooting.
  static bool IsRebootSupported();

  // The RebootShlib::RebootSource uses weakly typed enums, so this
  // can be used to validate a RebootSource value is actually valid.
  static bool IsValidRebootSource(RebootShlib::RebootSource reboot_source);

  // Before calling RebootNow, the caller must check that reboot is supported
  // for the RebootSource being attempted.
  static bool IsRebootSourceSupported(RebootShlib::RebootSource reboot_source);
  static bool RebootNow(RebootShlib::RebootSource reboot_source);

  // Before calling SetFdrForNextReboot, the caller must check that fdr
  // for next reboot is supported.
  static bool IsFdrForNextRebootSupported();
  static void SetFdrForNextReboot();

  // Before calling SetOtaForNextReboot, the caller must check that ota
  // for next reboot is supported.
  static bool IsOtaForNextRebootSupported();
  static void SetOtaForNextReboot();

  // Before calling IsClearOtaForNextRebootSupported, the called must check
  // if clearing the ota is supported.
  static bool IsClearOtaForNextRebootSupported();
  static void ClearOtaForNextReboot();

  // Returns last reboot source. This value persists throughout each boot.
  static RebootShlib::RebootSource GetLastRebootSource();

  // This is used for logging/metrics purposes. In general, setting the next
  // reboot type is handled automatically by RebootUtil, so it should not
  // be necessary to set explicitly.
  // Returns true if successful.
  static bool SetNextRebootSource(RebootShlib::RebootSource reboot_source);

  using RebootCallback =
      base::RepeatingCallback<bool(RebootShlib::RebootSource)>;
  // Sets a callback that will be fired when a reboot is requested. This is
  // used by tests to ensure that in production a reboot will occur. The
  // callback returns a bool which controls the return value of RebootNow()
  static void SetRebootCallbackForTest(const RebootCallback& callback);

  // Clears any previously set reboot callbacks. Should be used to clean up
  // the test after any invocation of SetRebootCallbackForTest
  static void ClearRebootCallbackForTest();
};

}  // namespace chromecast

#endif  // CHROMECAST_SYSTEM_REBOOT_REBOOT_UTIL_H_
