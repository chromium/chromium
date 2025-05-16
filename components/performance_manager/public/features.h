// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager::features {

#if !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() false
#else
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() true
#endif

// When enabled removes the rate limit on reporting tab processes to resourced.
#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kUnthrottledTabProcessReporting);
#endif

// Enable background tab loading of pages (restored via session restore)
// directly from Performance Manager rather than via TabLoader.
BASE_DECLARE_FEATURE(kBackgroundTabLoadingFromPerformanceManager);

// Minimum site engagement score for a tab to be restored, if it doesn't
// communicate in the background. If 0, engagement score doesn't prevent any tab
// from being loaded.
BASE_DECLARE_FEATURE_PARAM(size_t, kBackgroundTabLoadingMinSiteEngagement);

// If false, the background tab loading policy won't set the main frame restored
// state before restoring a tab. This gives it the same bugs as TabLoader: the
// notification permission and features stored in SiteDataReader won't be used,
// because they're looked up by url which isn't available without the restored
// state. This minimizes behaviour differences between TabLoader and the
// Performance Manager policy, for performance comparisons.
BASE_DECLARE_FEATURE_PARAM(bool, kBackgroundTabLoadingRestoreMainFrameState);

// Make the Battery Saver Modes available to users. If this is enabled, it
// doesn't mean the mode is enabled, just that the user has the option of
// toggling it.
BASE_DECLARE_FEATURE(kBatterySaverModeAvailable);

// Flags to control HaTS surveys about Chrome performance.
BASE_DECLARE_FEATURE(kPerformanceControlsPPMSurvey);

// Defines the minimum and maximum delay before showing the PPM survey. It will
// be shown the next time the user opens the New Tab Page after a random time in
// this range.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kPerformanceControlsPPMSurveyMinDelay);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kPerformanceControlsPPMSurveyMaxDelay);

// Controls whether survey responses will be tagged as "Selected For Uniform
// Sample". The subset of responses with this tag approximate the general
// population, no matter how many responses are received in individual segments.
BASE_DECLARE_FEATURE_PARAM(bool,
                           kPerformanceControlsPPMSurveyUniformSampleValue);

// Defines the names and boundaries of up to 3 segments for the PPM survey.
// There's no kPerformanceControlsPPMSurveySegmentMaxMemoryGB3 because there's
// never a 4th segment, so segment 3 has no maximum.
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kPerformanceControlsPPMSurveySegmentName1);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kPerformanceControlsPPMSurveySegmentName2);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kPerformanceControlsPPMSurveySegmentName3);
BASE_DECLARE_FEATURE_PARAM(size_t,
                           kPerformanceControlsPPMSurveySegmentMaxMemoryGB1);
BASE_DECLARE_FEATURE_PARAM(size_t,
                           kPerformanceControlsPPMSurveySegmentMaxMemoryGB2);

// This enables performance intervention to run in demo mode. While in demo
// mode, performance intervention will ignore rate throttling and CPU thresholds
// to make it easier to trigger performance intervention for testing purposes.
BASE_DECLARE_FEATURE(kPerformanceInterventionDemoMode);

// This enables performance intervention to use the improved notification
// prompting algorithm to show the intervention more often.
BASE_DECLARE_FEATURE(kPerformanceInterventionNotificationImprovements);

// Minimum time needed before showing another performance intervention.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kMinimumTimeBetweenReshow);

// Number of recent samples to be taken into consideration when determining
// performance intervention acceptance rate.
BASE_DECLARE_FEATURE_PARAM(int, kAcceptanceRateWindowSize);

// Upper bounds for showing performance intervention and will be scaled down
// based on the acceptance rate.
BASE_DECLARE_FEATURE_PARAM(int, kScaleMaxTimesPerDay);
BASE_DECLARE_FEATURE_PARAM(int, kScaleMaxTimesPerWeek);

// The amount of time a user needs to wait before being shown performance
// intervention with a 0% acceptance rate
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kNoAcceptanceBackOff);

// This enables performance intervention to use the updated notification
// strings.
BASE_DECLARE_FEATURE(kPerformanceInterventionNotificationStringImprovements);

// The version string that is used on the performance detection dialog.
BASE_DECLARE_FEATURE_PARAM(int, kNotificationStringVersion);

#endif

BASE_DECLARE_FEATURE(kPMProcessPriorityPolicy);

extern const base::FeatureParam<bool> kInheritParentPriority;

extern const base::FeatureParam<bool> kRenderedOutOfViewIsNotVisible;

extern const base::FeatureParam<bool> kNonSpareRendererHighInitialPriority;

BASE_DECLARE_FEATURE(kPMLoadingPageVoter);

// Policy that evicts the BFCache of pages that become non visible or the
// BFCache of all pages when the system is under memory pressure.
BASE_DECLARE_FEATURE(kBFCachePerformanceManagerPolicy);

// Whether tabs are discarded under high memory pressure.
BASE_DECLARE_FEATURE(kUrgentPageDiscarding);

// This represents the duration that CPU must be over the threshold before
// logging the delayed metrics.
extern const base::FeatureParam<base::TimeDelta> kDelayBeforeLogging;

// If Chrome CPU utilization is over the specified percent then we will log it.
extern const base::FeatureParam<int> kThresholdChromeCPUPercent;

// When enabled, the freezing policy measures background CPU usage.
BASE_DECLARE_FEATURE(kCPUMeasurementInFreezingPolicy);

// When enabled, the freezing policy measures memory usage. This exists to
// quantify the overhead of memory measurement in a holdback study.
BASE_DECLARE_FEATURE(kMemoryMeasurementInFreezingPolicy);

// When enabled, frozen browsing instances in which an origin's private memory
// footprint grows above a threshold are discarded. Depends on
// `kMemoryMeasurementInFreezingPolicy`.
BASE_DECLARE_FEATURE(kDiscardFrozenBrowsingInstancesWithGrowingPMF);

// Per-origin private memory footprint increase above which a frozen browsing
// instance is discarded.
BASE_DECLARE_FEATURE_PARAM(int, kFreezingMemoryGrowthThresholdToDiscardKb);

// Proportion of background CPU usage for a group of frames/workers that belong
// to the same [browsing instance, origin] that is considered "high".
BASE_DECLARE_FEATURE_PARAM(double, kFreezingHighCPUProportion);

// Time for which a page cannot be frozen after being visible.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kFreezingVisibleProtectionTime);

// Time for which a page cannot be frozen after being audible.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kFreezingAudioProtectionTime);

// When enabled, browsing instances with high CPU usage in background are frozen
// when Battery Saver is active. Depends on `kCPUMeasurementInFreezingPolicy`.
BASE_DECLARE_FEATURE(kFreezingOnBatterySaver);

// This is the similar to `kFreezingOnBatterySaver`, with some changes to
// facilitate testing:
// - Pretend that Battery Saver is active even if it's not.
// - Pretend that all tabs have high CPU usage in background.
BASE_DECLARE_FEATURE(kFreezingOnBatterySaverForTesting);

// When enabled, the freezing policy won't freeze pages that are opted out of
// tab discarding.
BASE_DECLARE_FEATURE(kFreezingFollowsDiscardOptOut);

// When enabled, the freezing eligibility UKM event may be recorded.
BASE_DECLARE_FEATURE(kRecordFreezingEligibilityUKM);

// When enabled, eligible tabs which are not in the N most recently used are
// frozen. This prevents CPU usage from growing proportionally with the number
// of tabs, and aims to make the browser support "infinite tabs" with good
// performance. A tab is eligible if it doesn't have a `CannotFreezeReason`
// other than `CannotFreezeReason::kRecentlyVisible`. N is configurable with
// `kInfiniteTabsFreezing_NumProtectedTabs`. Tabs frozen by this feature are
// periodically unfrozen, to allow showing notifications, refreshing content,
// maintaining connections... (see `kInfiniteTabsFreezing_UnfreezeInterval` and
// `kInfiniteTabsFreezing_UnfreezeDuration`).
BASE_DECLARE_FEATURE(kInfiniteTabsFreezing);

// Number of most recently visible tabs protected from "infinite tabs" freezing.
BASE_DECLARE_FEATURE_PARAM(int, kInfiniteTabsFreezing_NumProtectedTabs);

// Interval at which tabs frozen to support "infinite tabs" are temporarily
// unfrozen.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kInfiniteTabsFreezing_UnfreezeInterval);

// Duration for which tabs frozen to support "infinite tabs" are temporarily
// unfrozen.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kInfiniteTabsFreezing_UnfreezeDuration);

// When enabled, Resource Attribution measurements will include contexts for
// individual origins.
BASE_DECLARE_FEATURE(kResourceAttributionIncludeOrigins);

// When enabled, change the ordering of frame swap in render (crbug/357649043).
BASE_DECLARE_FEATURE(kSeamlessRenderFrameSwap);

// When enabled, visible unimportant frames receives a lesser priority than
// non unimportant frames.
BASE_DECLARE_FEATURE(kUnimportantFramesPriority);

// When enabled, the begin frame rate of visible unimportant frames would be
// reduced to half of normal frame rate.
BASE_DECLARE_FEATURE(kThrottleUnimportantFrameRate);

// When enabled, keep the default search engine render process host alive
// (crbug.com/365958798).
BASE_DECLARE_FEATURE(kKeepDefaultSearchEngineRendererAlive);

}  // namespace performance_manager::features

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
