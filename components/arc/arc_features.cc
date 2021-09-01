// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_features.h"

namespace arc {

// Controls ACTION_BOOT_COMPLETED broadcast for third party applications on ARC.
// When disabled, third party apps will not receive this broadcast.
const base::Feature kBootCompletedBroadcastFeature {
    "ArcBootCompletedBroadcast", base::FEATURE_ENABLED_BY_DEFAULT
};

// Controls experimental Custom Tabs feature for ARC.
const base::Feature kCustomTabsExperimentFeature{
    "ArcCustomTabsExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to handle files with unknown size.
const base::Feature kDocumentsProviderUnknownSizeFeature{
    "ArcDocumentsProviderUnknownSize", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to pass throttling notifications to Android side.
const base::Feature kEnableThrottlingNotification{
    "ArcEnableThrottlingNotification", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we should delegate audio focus requests from ARC to Chrome.
const base::Feature kEnableUnifiedAudioFocusFeature{
    "ArcEnableUnifiedAudioFocus", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether ARC handles unmanaged->managed account transition.
const base::Feature kEnableUnmanagedToManagedTransitionFeature{
    "ArcEnableUnmanagedToManagedTransitionFeature",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls ARC Unspecialized Application Processes.
// When enabled, Android creates a pool of processes
// that will start applications so that zygote doesn't have to wake.
const base::Feature kEnableUsap{"ArcEnableUsap",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether ARC apps can share to Web Apps through WebAPKs and TWAs.
const base::Feature kEnableWebAppShareFeature{"ArcEnableWebAppShare",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Controls experimental file picker feature for ARC.
const base::Feature kFilePickerExperimentFeature{
    "ArcFilePickerExperiment", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls image copy & paste app compat feature in ARC.
const base::Feature kImageCopyPasteCompatFeature{
    "ArcImageCopyPasteCompat", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls keyboard shortcut helper integration feature in ARC.
const base::Feature kKeyboardShortcutHelperIntegrationFeature{
    "ArcKeyboardShortcutHelperIntegration", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls experimental 64-bit native bridge support for ARC on boards that
// have 64-bit native bridge support available but not yet enabled.
const base::Feature kNativeBridge64BitSupportExperimentFeature{
    "ArcNativeBridge64BitSupportExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Toggles between native bridge implementations for ARC.
// Note, that we keep the original feature name to preserve
// corresponding metrics.
const base::Feature kNativeBridgeToggleFeature{
    "ArcNativeBridgeExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls ARC picture-in-picture feature. If this is enabled, then Android
// will control which apps can enter PIP. If this is disabled, then ARC PIP
// will be disabled.
const base::Feature kPictureInPictureFeature{"ArcPictureInPicture",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, tracing raw files are saved in order to help debug failures.
const base::Feature kSaveRawFilesOnTracing{"ArcSaveRawFilesOnTracing",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Controls ARCVM real time vcpu feature on a device with 2 logical cores
// online.
const base::Feature kRtVcpuDualCore{"ArcRtVcpuDualCore",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls ARCVM real time vcpu feature on a device with 3+ logical cores
// online.
const base::Feature kRtVcpuQuadCore{"ArcRtVcpuQuadCore",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls ARC high-memory dalvik profile in ARCVM.
// When enabled, Android tries to use dalvik memory profile tuned for
// high-memory devices like 8G and 16G. This is enabled without conditions
// in ARC container.
const base::Feature kUseHighMemoryDalvikProfile{
    "ArcUseHighMemoryDalvikProfile", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls ARC USB Storage UI feature.
// When enabled, chrome://settings and Files.app will ask if the user wants
// to expose USB storage devices to ARC.
const base::Feature kUsbStorageUIFeature{"ArcUsbStorageUI",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether ARC uses VideoDecoder-backed video decoding.
// When enabled, GpuArcVideoDecodeAccelerator will use VdVideoDecodeAccelerator
// to delegate decoding tasks to VideoDecoder implementations, instead of using
// VDA implementations created by GpuVideoDecodeAcceleratorFactory.
const base::Feature kVideoDecoder{"ArcVideoDecoder",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether a custom memory size is used when creating ARCVM. When
// enabled, ARCVM is sized with the following formula:
//  min(max_mib, RAM + shift_mib)
// If disabled, memory is sized by concierge which, at the time of writing, uses
// RAM - 1024 MiB.
const base::Feature kVmMemorySize{"ArcVmMemorySize",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the amount to "shift" system RAM when sizing ARCVM. The default
// value of 0 means that ARCVM's memory will be thr same as the system.
const base::FeatureParam<int> kVmMemorySizeShiftMiB{&kVmMemorySize, "shift_mib",
                                                    0};

// Controls the maximum amount of memory to give ARCVM. The default value of
// INT32_MAX means that ARCVM's memory is not capped.
const base::FeatureParam<int> kVmMemorySizeMaxMiB{&kVmMemorySize, "max_mib",
                                                  INT32_MAX};

// Controls whether to use the new limit cache balloon policy. If disabled the
// old balance available balloon policy is used. If enabled, ChromeOS's Resource
// Manager (resourced) is able to kill ARCVM apps by sending a memory pressure
// signal.
// The limit cache balloon policy inflates the balloon to limit the kernel page
// cache inside ARCVM if memory in the host is low. See FeatureParams below for
// the conditions that limit cache. See mOomMinFreeHigh and mOomAdj in
// frameworks/base/services/core/java/com/android/server/am/ProcessList.java
// to see how LMKD maps kernel page cache to a priority level of app to kill.
// To ensure fairness between tab manager discards and ARCVM low memory kills,
// we want to stop LMKD killing things out of turn. We do this by making sure
// ARCVM never has it's kernel page cache drop below the level that LMKD will
// start killing.
const base::Feature kVmBalloonPolicy{"ArcVmBalloonPolicy",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// The maximum amount of kernel page cache ARCVM can have when ChromeOS is under
// moderate memory pressure. 0 for no limit.
const base::FeatureParam<int> kVmBalloonPolicyModerateKiB{&kVmBalloonPolicy,
                                                          "moderate_kib", 0};

// The maximum amount of kernel page cache ARCVM can have when ChromeOS is under
// critical memory pressure. 0 for no limit. The default value of 184320KiB
// corresponds to the level LMKD will start to kill the lowest priority cached
// app.
const base::FeatureParam<int> kVmBalloonPolicyCriticalKiB{
    &kVmBalloonPolicy, "critical_kib", 184320};

// The maximum amount of kernel page cache ARCVM can have when ChromeOS is
// reclaiming. 0 for no limit. The default value of 184320KiB corresponds to the
// level LMKD will start to kill the lowest priority cached app.
const base::FeatureParam<int> kVmBalloonPolicyReclaimKiB{&kVmBalloonPolicy,
                                                         "reclaim_kib", 184320};

}  // namespace arc
