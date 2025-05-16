// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for ARC.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_ARC_FEATURES_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_ARC_FEATURES_H_

#include <base/time/time.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace arc {

// Please keep alphabetized.
BASE_DECLARE_FEATURE(kArcExchangeVersionOnMojoHandshake);
BASE_DECLARE_FEATURE(kArcOnDemandV2);
BASE_DECLARE_FEATURE_PARAM(bool, kArcOnDemandActivateOnAppLaunch);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kArcOnDemandInactiveInterval);
BASE_DECLARE_FEATURE(kArcVmGki);
BASE_DECLARE_FEATURE(kBlockIoScheduler);
BASE_DECLARE_FEATURE_PARAM(bool, kEnableDataBlockIoScheduler);
BASE_DECLARE_FEATURE(kBootCompletedBroadcastFeature);
BASE_DECLARE_FEATURE(kCustomTabsExperimentFeature);
BASE_DECLARE_FEATURE(kDeferArcActivationUntilUserSessionStartUpTaskCompletion);
BASE_DECLARE_FEATURE_PARAM(int, kDeferArcActivationHistoryWindow);
BASE_DECLARE_FEATURE_PARAM(int, kDeferArcActivationHistoryThreshold);
BASE_DECLARE_FEATURE(kEnableArcAttestation);
BASE_DECLARE_FEATURE(kEnableArcIdleManager);
BASE_DECLARE_FEATURE_PARAM(bool, kEnableArcIdleManagerIgnoreBatteryForPLT);
BASE_DECLARE_FEATURE_PARAM(int, kEnableArcIdleManagerDelayMs);
BASE_DECLARE_FEATURE_PARAM(bool, kEnableArcIdleManagerPendingIdleReactivate);
BASE_DECLARE_FEATURE(kEnableArcS2Idle);
BASE_DECLARE_FEATURE(kEnableArcVmDataMigration);
BASE_DECLARE_FEATURE(kEnableFriendlierErrorDialog);
BASE_DECLARE_FEATURE(kEnablePerVmCoreScheduling);
BASE_DECLARE_FEATURE(kEnableVirtioBlkForData);
BASE_DECLARE_FEATURE(kEnableVirtioBlkMultipleWorkers);
BASE_DECLARE_FEATURE(kExtendIntentAnrTimeout);
BASE_DECLARE_FEATURE(kExtendServiceAnrTimeout);
BASE_DECLARE_FEATURE(kExternalStorageAccess);
BASE_DECLARE_FEATURE(kGmsCoreLowMemoryKillerProtection);
BASE_DECLARE_FEATURE(kGuestSwap);
BASE_DECLARE_FEATURE_PARAM(int, kGuestSwapSize);
BASE_DECLARE_FEATURE_PARAM(int, kGuestZramSizePercentage);
BASE_DECLARE_FEATURE_PARAM(int, kGuestZramSwappiness);
BASE_DECLARE_FEATURE_PARAM(bool, kGuestReclaimEnabled);
BASE_DECLARE_FEATURE_PARAM(bool, kGuestReclaimOnlyAnonymous);
BASE_DECLARE_FEATURE_PARAM(bool, kVirtualSwapEnabled);
BASE_DECLARE_FEATURE_PARAM(int, kVirtualSwapIntervalMs);
BASE_DECLARE_FEATURE(kArcVmPvclock);
BASE_DECLARE_FEATURE(kLockGuestMemory);
BASE_DECLARE_FEATURE(kLvmApplicationContainers);
BASE_DECLARE_FEATURE(kNativeBridgeToggleFeature);
BASE_DECLARE_FEATURE(kOutOfProcessVideoDecoding);
BASE_DECLARE_FEATURE(kPerAppLanguage);
BASE_DECLARE_FEATURE(kResizeCompat);
BASE_DECLARE_FEATURE(kRoundedWindowCompat);
extern const char kRoundedWindowCompatStrategy[];
extern const char kRoundedWindowCompatStrategy_BottomOnlyGesture[];
extern const char kRoundedWindowCompatStrategy_LeftRightBottomGesture[];
BASE_DECLARE_FEATURE(kRtVcpuDualCore);
BASE_DECLARE_FEATURE(kRtVcpuQuadCore);
BASE_DECLARE_FEATURE(kSaveRawFilesOnTracing);
BASE_DECLARE_FEATURE(kSkipDropCaches);
BASE_DECLARE_FEATURE(kSwitchToKeyMintOnTOverride);
BASE_DECLARE_FEATURE(kSyncInstallPriority);
BASE_DECLARE_FEATURE(kUnthrottleOnActiveAudioV2);
BASE_DECLARE_FEATURE(kVideoDecoder);
BASE_DECLARE_FEATURE(kVmMemoryPSIReports);
BASE_DECLARE_FEATURE_PARAM(int, kVmMemoryPSIReportsPeriod);
BASE_DECLARE_FEATURE(kVmMemorySize);
BASE_DECLARE_FEATURE_PARAM(int, kVmMemorySizeShiftMiB);
BASE_DECLARE_FEATURE_PARAM(int, kVmMemorySizeMaxMiB);
BASE_DECLARE_FEATURE_PARAM(int, kVmMemorySizePercentage);
BASE_DECLARE_FEATURE(kVmmSwapoutGhostWindow);
BASE_DECLARE_FEATURE(kVmmSwapKeyboardShortcut);
BASE_DECLARE_FEATURE(kVmmSwapPolicy);
BASE_DECLARE_FEATURE_PARAM(int, kVmmSwapOutDelaySecond);
BASE_DECLARE_FEATURE_PARAM(int, kVmmSwapOutTimeIntervalSecond);
BASE_DECLARE_FEATURE_PARAM(int, kVmmSwapArcSilenceIntervalSecond);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kVmmSwapTrimInterval);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kVmmSwapMinShrinkInterval);
BASE_DECLARE_FEATURE(kLmkPerceptibleMinStateUpdate);
}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_ARC_FEATURES_H_
