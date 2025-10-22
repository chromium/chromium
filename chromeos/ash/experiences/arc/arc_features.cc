// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/arc_features.h"

#include "base/feature_list.h"

namespace arc {

// When enabled, the versions of ChromeOS and ARC are exchanged during
// handshake. This feature reduces unnecessary inter-process communications.
BASE_FEATURE(kArcExchangeVersionOnMojoHandshake,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to always start ARC automatically, or wait for the user's
// action to start it later in an on-demand manner. Already enabled by default
// for managed users. In V2, it will be expand to more users such as unmanaged
// users.
BASE_FEATURE(kArcOnDemandV2, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARC should be activated on any app launches. If set to
// false, inactive_interval will be checked.
BASE_FEATURE_PARAM(bool,
                   kArcOnDemandActivateOnAppLaunch,
                   &kArcOnDemandV2,
                   "activate_on_app_launch",
                   false);

// Controls how long of invactivity are allowed before ARC on Demand is
// triggered.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kArcOnDemandInactiveInterval,
                   &kArcOnDemandV2,
                   "inactive_interval",
                   base::Days(7));

// Controls whether to start ARC with the GKI kernel.
BASE_FEATURE(kArcVmGki, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls block IO schedulers in ARCVM.
BASE_FEATURE(kBlockIoScheduler,
             "ArcBlockIoScheduler",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable block IO scheduler for virtio-blk /data.
BASE_FEATURE_PARAM(bool,
                   kEnableDataBlockIoScheduler,
                   &kBlockIoScheduler,
                   "data_block_io_scheduler",
                   true);

// Controls ACTION_BOOT_COMPLETED broadcast for third party applications on ARC.
// When disabled, third party apps will not receive this broadcast.
BASE_FEATURE(kBootCompletedBroadcastFeature,
             "ArcBootCompletedBroadcast",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Defers the ARC actvation until the user session start up tasks
// are completed to give more resources to critical tasks for user session
// starting.
BASE_FEATURE(kDeferArcActivationUntilUserSessionStartUpTaskCompletion,
             base::FEATURE_ENABLED_BY_DEFAULT);

// We decide whether to defer ARC activation by taking a look at recent
// user activities. If the user activates ARC soon after user session start
// recently, ARC will be immediately activated when ready in following
// sessions.
// The details are configured by these two variables; history_window and
// history_threshold. If the user activates ARC soon after the user session
// starts more than or equal to `history_threshold` sessions in recent
// `history_window` sessions, ARC will be launched immediately.
// Note: if `history_threshold` > `history_window`, as it will never be
// satisfied, ARC will be always deferred.
BASE_FEATURE_PARAM(int,
                   kDeferArcActivationHistoryWindow,
                   &kDeferArcActivationUntilUserSessionStartUpTaskCompletion,
                   "history_window",
                   5);
BASE_FEATURE_PARAM(int,
                   kDeferArcActivationHistoryThreshold,
                   &kDeferArcActivationUntilUserSessionStartUpTaskCompletion,
                   "history_threshold",
                   3);

// Controls whether attestation will be used on ARCVM.
BASE_FEATURE(kEnableArcAttestation,
             "ArcAttestation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether we automatically send ARCVM into Doze mode
// when it is mostly idle - even if Chrome is still active.
BASE_FEATURE(kEnableArcIdleManager,
             "ArcIdleManager",
             base::FEATURE_ENABLED_BY_DEFAULT);

// For test purposes, ignore battery status changes, allowing Doze mode to
// kick in even if we do not receive powerd changes related to battery.
BASE_FEATURE_PARAM(bool,
                   kEnableArcIdleManagerIgnoreBatteryForPLT,
                   &kEnableArcIdleManager,
                   "ignore_battery_for_test",
                   true);

BASE_FEATURE_PARAM(int,
                   kEnableArcIdleManagerDelayMs,
                   &kEnableArcIdleManager,
                   "delay_ms",
                   360 * 1000);

BASE_FEATURE_PARAM(bool,
                   kEnableArcIdleManagerPendingIdleReactivate,
                   &kEnableArcIdleManager,
                   "pending_idle_reactivate",
                   false);

// Controls whether to enable support for s2idle in ARCVM.
BASE_FEATURE(kEnableArcS2Idle, "ArcS2Idle", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable ARCVM /data migration. It does not take effect
// when kEnableVirtioBlkForData is set, in which case virtio-blk is used for
// /data without going through the migration.
BASE_FEATURE(kEnableArcVmDataMigration,
             "ArcVmDataMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable friendlier error dialog (switching to notification
// for certain types of ARC error dialogs).
BASE_FEATURE(kEnableFriendlierErrorDialog,
             "FriendlierErrorDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether crosvm for ARCVM does per-VM core scheduling on devices with
// MDS/L1TF vulnerabilities. When this feature is disabled, crosvm does per-vCPU
// core scheduling which is more secure.
//
// How to safely disable this feature for security (or other) reasons:
//
// 1) Visit go/stainless and verify arc.Boot.vm_with_per_vcpu_core_scheduling is
//    green (it should always be because arc.Boot is a critical test.)
// 2) Change the default value of this feature to FEATURE_DISABLED_BY_DEFAULT.
// 3) Monitor arc.Boot.vm at go/stainless after Chrome is rolled.
// 4) Ask ARC team (//chromeos/ash/experiences/arc/OWNERS) to update
// arc.CPUSet.vm test
//    so the Tast test uses the updated ArcEnablePerVmCoreScheduling setting.
BASE_FEATURE(kEnablePerVmCoreScheduling,
             "ArcEnablePerVmCoreScheduling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use virtio-blk for Android /data instead of using
// virtio-fs.
BASE_FEATURE(kEnableVirtioBlkForData,
             "ArcEnableVirtioBlkForData",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable the multiple-worker feature in virtio-blk disks
BASE_FEATURE(kEnableVirtioBlkMultipleWorkers,
             "ArcEnableVirtioBlkMultipleWorkers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to extend the broadcast of intent ANR timeout time.
BASE_FEATURE(kExtendIntentAnrTimeout,
             "ArcExtendIntentAnrTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to extend the executing service ANR timeout time.
BASE_FEATURE(kExtendServiceAnrTimeout,
             "ArcExtendServiceAnrTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to allow Android apps to access external storage devices
// like USB flash drives and SD cards.
BASE_FEATURE(kExternalStorageAccess,
             "ArcExternalStorageAccess",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Indicates whether LVM application containers feature is supported.
BASE_FEATURE(kLvmApplicationContainers,
             "ArcLvmApplicationContainers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the guest swap is enabled. This is only for ARCVM.
BASE_FEATURE(kGuestSwap, "ArcGuestZram", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the size of the guest swap area by an absolute value. Ignored if
// "size_percentage" is set.
BASE_FEATURE_PARAM(int, kGuestSwapSize, &kGuestSwap, "size", 0);

// Controls the size of the guest swap area by a percentage of the VM memory
// size.
BASE_FEATURE_PARAM(int,
                   kGuestZramSizePercentage,
                   &kGuestSwap,
                   "size_percentage",
                   0);

// Controls swappiness for the ARCVM guest.
BASE_FEATURE_PARAM(int, kGuestZramSwappiness, &kGuestSwap, "swappiness", 0);

// Controls whether to do per-process reclaim from the ARCVM guest.
BASE_FEATURE_PARAM(bool,
                   kGuestReclaimEnabled,
                   &kGuestSwap,
                   "guest_reclaim_enabled",
                   false);

// Controls whether only anonymous pages are reclaimed from the ARCVM guest.
// Ignored when the "guest_reclaim_enabled" param is false.
BASE_FEATURE_PARAM(bool,
                   kGuestReclaimOnlyAnonymous,
                   &kGuestSwap,
                   "guest_reclaim_only_anonymous",
                   false);

// Controls whether to enable virtual swap device for ARCVM.
BASE_FEATURE_PARAM(bool,
                   kVirtualSwapEnabled,
                   &kGuestSwap,
                   "virtual_swap_enabled",
                   false);

// Controls how often ARCVM's virtual swap device is swapped out in the host.
BASE_FEATURE_PARAM(int,
                   kVirtualSwapIntervalMs,
                   &kGuestSwap,
                   "virtual_swap_interval_ms",
                   1000);

// Controls whether to enable virtio-pvclock in ARCVM
BASE_FEATURE(kArcVmPvclock,
             "ArcEnablePvclock",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables/disables mlock() of guest memory for ARCVM.
// Often used in combination with kGuestZram.
BASE_FEATURE(kLockGuestMemory,
             "ArcLockGuestMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Toggles between native bridge implementations for ARC.
// Note, that we keep the original feature name to preserve
// corresponding metrics.
BASE_FEATURE(kNativeBridgeToggleFeature,
             "ArcNativeBridgeExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, utility processes are spawned to perform hardware decode
// acceleration on behalf of ARC++/ARCVM instead of using the GPU process.
BASE_FEATURE(kOutOfProcessVideoDecoding, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Android per-app-language settings will be surfaced in ChromeOS
// Settings page.
BASE_FEATURE(kPerAppLanguage, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResizeCompat,
             "ArcResizeCompat",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 2 logical cores
// online.
// When you change the default, you also need to change the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// match it to the new default.
BASE_FEATURE(kRtVcpuDualCore,
             "ArcRtVcpuDualCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 3+ logical cores
// online.
// When you change the default, you also need to modify the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// add ArcRtVcpuQuadCore there. Otherwise, the test will start failing.
BASE_FEATURE(kRtVcpuQuadCore,
             "ArcRtVcpuQuadCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, tracing raw files are saved in order to help debug failures.
BASE_FEATURE(kSaveRawFilesOnTracing,
             "ArcSaveRawFilesOnTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, skip dropping ARCVM page cache after boot.
BASE_FEATURE(kSkipDropCaches,
             "ArcSkipDropPageCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, ARC will pass install priority to Play in sync install
// requests.
BASE_FEATURE(kSyncInstallPriority,
             "ArcSyncInstallPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, ARC will not be throttled when there is active audio stream
// from ARC.
BASE_FEATURE(kUnthrottleOnActiveAudioV2,
             "ArcUnthrottleOnActiveAudioV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether ARC uses VideoDecoder-backed video decoding.
// When enabled, GpuArcVideoDecodeAccelerator will use VdVideoDecodeAccelerator
// to delegate decoding tasks to VideoDecoder implementations, instead of using
// VDA implementations created by GpuVideoDecodeAcceleratorFactory.
BASE_FEATURE(kVideoDecoder,
             "ArcVideoDecoder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARC uses MappableSharedImage for video encoding.
BASE_FEATURE(kVideoEncodeUseMappableSI, base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to continuously log PSI memory pressure data to Chrome.
BASE_FEATURE(kVmMemoryPSIReports,
             "ArcVmMemoryPSIReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls how frequently memory pressure data is logged
BASE_FEATURE_PARAM(int,
                   kVmMemoryPSIReportsPeriod,
                   &kVmMemoryPSIReports,
                   "period",
                   10);

// Controls whether a custom memory size is used when creating ARCVM. When
// enabled, ARCVM is sized with the following formula:
//  min(max_mib, ram_percentage / 100 * RAM + shift_mib)
// If disabled, memory is sized by concierge which, at the time of writing, uses
// RAM - 1024 MiB.
BASE_FEATURE(kVmMemorySize,
             "ArcVmMemorySize",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the amount to "shift" system RAM when sizing ARCVM. The default
// value of 0 means that ARCVM's memory will be thr same as the system.
BASE_FEATURE_PARAM(int,
                   kVmMemorySizeShiftMiB,
                   &kVmMemorySize,
                   "shift_mib",
                   -500);

// Controls the maximum amount of memory to give ARCVM. The default value of
// INT32_MAX means that ARCVM's memory is not capped.
BASE_FEATURE_PARAM(int,
                   kVmMemorySizeMaxMiB,
                   &kVmMemorySize,
                   "max_mib",
                   INT32_MAX);

// Controls the percentage of system RAM for calculation of ARCVM size. The
// default value of 100 means the whole system RAM will be used in ARCM size
// calculation.
BASE_FEATURE_PARAM(int,
                   kVmMemorySizePercentage,
                   &kVmMemorySize,
                   "ram_percentage",
                   100);

// Controls experimental key to enable ghost window when launch app under ARCVM
// swap out state.
BASE_FEATURE(kVmmSwapoutGhostWindow,
             "ArcVmmSwapoutGhostWindow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental key to enable Vmm swap for ARCVM by keyboard shortcut.
BASE_FEATURE(kVmmSwapKeyboardShortcut,
             "ArcvmSwapoutKeyboardShortcut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental key to enable and swap out ARCVM by policy.
BASE_FEATURE(kVmmSwapPolicy,
             "ArcVmmSwapPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the time interval between create staging memory and swap out. The
// default value is 10 seconds.
BASE_FEATURE_PARAM(int,
                   kVmmSwapOutDelaySecond,
                   &kVmmSwapPolicy,
                   "delay_sec",
                   10);

// Controls the time interval between two swap out. The default value is 12
// hours.
BASE_FEATURE_PARAM(int,
                   kVmmSwapOutTimeIntervalSecond,
                   &kVmmSwapPolicy,
                   "swapout_interval_sec",
                   60 * 60 * 12);

// Controls the time interval of ARC silence. The default value is 15 minutes.
BASE_FEATURE_PARAM(int,
                   kVmmSwapArcSilenceIntervalSecond,
                   &kVmmSwapPolicy,
                   "arc_silence_interval_sec",
                   60 * 15);

// Controls the interval for swap trimming maintenance.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVmmSwapTrimInterval,
                   &kVmmSwapPolicy,
                   "swap_trim_interval",
                   base::Hours(1));

// Controls the minimum time interval between attempts to shrink ARCVM memory
// when swap is enabled or swap trimming is performed.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVmmSwapMinShrinkInterval,
                   &kVmmSwapPolicy,
                   "min_shrink_interval",
                   base::Minutes(10));

// Controls the feature to update the minimum Android process state to be
// considered to be killed under perceptible memory pressure. This is to prevent
// top Android apps from being killed that result in bad user experience.
BASE_FEATURE(kLmkPerceptibleMinStateUpdate,
             "ArcLmkPerceptibleMinStateUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace arc
