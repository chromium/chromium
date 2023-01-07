// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_REBOOT_SHLIB_H_
#define CHROMECAST_PUBLIC_REBOOT_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {

// All methods in the RebootShlib interface can be called from any thread.
class CHROMECAST_EXPORT RebootShlib {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.cast
  enum RebootSource {
    // A default value to use if the source of a reboot is not known.
    UNKNOWN = 0,

    // A reboot that wasn't device-initiated (e.g. power cycle).
    FORCED = 1,

    // A reboot triggered by the setup API that was not an FDR or an OTA.
    // This is a user initiated reboot attempt.
    API = 2,

    // An automatic reboot of the device that happens once a day.
    NIGHTLY = 3,

    // A reboot caused by an OTA/update related source.
    // Note: Calling RebootNow(RebootSource::OTA) does not imply that
    // an OTA must happen upon reboot; it is simply an indicator that a reboot
    // request is coming from an OTA-related source.
    // Only if SetOtaForNextReboot() has been called must reboot to result
    // in an OTA update.
    OTA = 4,

    // A reboot caused by a watchdog process, which generally indicates
    // a device malfunction of some sort.
    WATCHDOG = 5,

    // A reboot triggered by the Cast ProcessManager, which monitors various
    // cast services and will cause a reboot if critical services are failing.
    PROCESS_MANAGER = 6,

    // A reboot triggered by the Cast CrashUploader, which is responsible for
    // handling crash reports and will cause a reboot if a high crash rate is
    // detected.
    CRASH_UPLOADER = 7,

    // A reboot caused by an FDR related source.
    // Note: Calling RebootNow(RebootSource::FDR) does not imply that
    // an FDR must happen upon reboot; it is simply an indicator that a reboot
    // request is coming from an FDR-related source.
    // Only if SetFdrForNextReboot() has been called must reboot to result
    // in an OTA update.
    FDR = 8,

    // A reboot caused by hardware watchdog. This requires additional
    // information from the SoC. Otherwise, it can be categorized as forced.
    HW_WATCHDOG = 9,

    // A reboot caused by other software reason that is not listed above.
    SW_OTHER = 10,

    // A reboot caused by overheat.
    OVERHEAT = 11,

    // The device got into a state such that it needs to regenerate the cloud
    // device id.
    REGENERATE_CLOUD_ID = 12,

    // A reboot is triggered due to successive OOM events.
    REPEATED_OOM = 13,

    // A reboot is triggered when the utility process is found to be in
    // crash loop.
    UTILITY_PROCESS_CRASH = 14,

    // Restart of the Cast component following a graceful teardown, e.g.,
    // intended by the platform. Device has not rebooted. This only makes sense
    // on platforms that allow a component restart without a full device reboot.
    GRACEFUL_RESTART = 15,

    // Restart of the Cast component following an ungraceful teardown, e.g.,
    // a crash.. Device has not rebooted. This only makes sense on platforms
    // that allow a component restart without a full device reboot.
    UNGRACEFUL_RESTART = 16,

    // A dirty reboot is triggered due to multiple services are affected by
    // bugs. In that case, business logic can't proceed and busy references
    // can't be trusted, so a dirty reboot will be executed.
    MULTI_SERVICE_BUG = 17,

    // Intentional shutdown by power manager of battery powered devices.
    POWER_MANAGER_SHUTDOWN = 18,

    // Restart of the Cast component to apply changes due to an experiment flag
    // value change. This is only used to handle flag changes that would be more
    // risky to attempt without a full process restart. Very few experiments
    // trigger this path.
    EXPERIMENT_CHANGE = 19,

    // A reboot is triggered when the anomaly detection service detects a
    // system-level anomaly and needs to recover from it with restarting the
    // Cast component.
    ANOMALY = 20,

    // A reboot is triggered when the kernel panics.
    KERNEL_PANIC = 21,
  };

  // Initializes any platform-specific reboot systems.
  static void Initialize(const std::vector<std::string>& argv);

  // Tears down and uninitializes any platform-specific reboot systems.
  static void Finalize();

  // Returns whether this shlib is supported. If this returns true, it
  // indicates that IsRebootSourceSupported will be true for at least one
  // RebootSource.
  static bool IsSupported();

  // Indicates if a particular RebootSource is supported. If |reboot_source|
  // is supported, calling RebootNow(|reboot_source|) should cause the system
  // to reboot as soon as possible.
  static bool IsRebootSourceSupported(RebootSource reboot_source);

  // Causes the system to reboot as soon as possible if the supplied
  // |reboot_source| is supported.
  // Returns true if the reboot is expected to succeed.
  // Returns false if there is an error during reboot attempt (or the
  // |reboot_source| is not supported).
  //
  // NOTE: RebootNow(RebootSource::FDR) does NOT require an FDR to occur upon
  // reboot. An FDR is only required to occur upon reboot if
  // IsFdrForNextRebootSupported() returns true and SetFdrForNextReboot()
  // has been called. If SetFdrForNextReboot() has been called, then a reboot
  // must result in an FDR regardless of the RebootSource passed to RebootNow.
  //
  // NOTE: RebootNow(RebootSource::OTA) does NOT require an OTA to occur upon
  // reboot. An OTA is only required to occur upon reboot if
  // IsOtaForNextRebootSupported() returns true and SetOtaForNextReboot()
  // has been called. If SetOtaForNextReboot() has been called, then a reboot
  // must result in an OTA regardless of the RebootSource passed to RebootNow.
  static bool RebootNow(RebootSource reboot_source);

  // Indicates if FDR (Factory Data Reset) on next reboot is supported.
  // NOTE: This is independent from IsRebootSourceSupported(RebootSource::FDR).
  // If IsFdrForNextRebootSupported() returns true and SetFdrForNextReboot()
  // has been called, then the device must do an FDR upon reboot, regardless
  // of the type of reboot (including power cycle).
  static bool IsFdrForNextRebootSupported();

  // If IsFdrSupported() returns true, then calling SetFdrForNextReboot()
  // must result in FDR occuring upon the next reboot (regardless of the
  // RebootSource or cause of reboot).
  static void SetFdrForNextReboot();

  // Indicates if updates (also known as OTAs) on next reboot are supported.
  // NOTE: This is independent from IsRebootSourceSupported(RebootSource::OTA).
  // If IsOtaForNextRebootSupported() returns true and SetOtaForNextReboot()
  // has been called, then the device must do an OTA upon reboot, regardless
  // of the type of reboot (including power cycle).
  static bool IsOtaForNextRebootSupported();

  // If IsOtaSupported() returns true, then calling SetOtaForNextReboot()
  // must result in any available OTA update getting applied upon the next
  // reboot (regardless of the RebootSource or cause of reboot).
  static void SetOtaForNextReboot();

  // If IsClearOtaFromNextRebootSupported() returns true and
  // ClearOtaForNextReboot() has been called, then the device will cancel any
  // pending OTA waiting to be applied.
  static bool IsClearOtaForNextRebootSupported();

  // If IsClearOtaForNextRebootSupported() returns true then calling
  // ClearOtaForNextReboot voids any prior call to SetOtaForNextReboot.
  static void ClearOtaForNextReboot();
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_REBOOT_SHLIB_H_
