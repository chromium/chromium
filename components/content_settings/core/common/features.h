// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace content_settings {

#if BUILDFLAG(IS_ANDROID)
// Enables auto dark feature in theme settings.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kDarkenWebsitesCheckboxInThemesSetting);
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut;
#endif

namespace features {

// Feature to enable the unused site permissions module of Safety Check.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kSafetyCheckUnusedSitePermissions);

// When enabled, allowlisted website settings are considered for Safety Check,
// in addition to content settings that are included by default.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(
    kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions);

// Lets the HostContentSettingsMap actively monitor when content settings expire
// and delete them instantly. This also notifies observers that will, in turn,
// terminate access to capabilities gated on those settings right away.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kActiveContentSettingExpiry);

// When enabled, site permissions will be considered as unused immediately in
// order to facilitate testing.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay;

// When enabled, site permissions will be considered as unused after a smaller
// delay in order to facilitate testing.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool>
    kSafetyCheckUnusedSitePermissionsWithDelay;

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kApproximateGeolocationPermission);

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<int> kApproximateGeolocationPermissionPromptArm;

// Feature to enable the feedback button in the User Bypass UI.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kUserBypassFeedback);

// Feature to enable the User Bypass UI.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kUserBypassUI);

// Determines the time interval after which a user bypass exception expires.
// Note that it affects only new exceptions, previously created exceptions won't
// be updated to use a new expiration.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kUserBypassUIExceptionExpiration;

// Determines how many refreshes within `kUserBypassUIReloadTime` are required
// before a high confidence signal is returned.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<int> kUserBypassUIReloadCount;

// Determines how long a user has to make `kUserBypassUIReloadCount` refreshes
// before a high confidence signal is returned.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kUserBypassUIReloadTime;

// The reloading bubble will be shown until either the page full reloads or this
// timeout is reached.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kUserBypassUIReloadBubbleTimeout;

// Move activity indicators to the left-hand side of Omnibox.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kLeftHandSideActivityIndicators);

#if BUILDFLAG(IS_CHROMEOS)
// Shows warnings if camera, microphone or geolocation is blocked in the OS.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kCrosSystemLevelPermissionBlockedWarnings);
#endif

// Feature to enable redesigned tracking protection UX + prefs for 3PCD.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtection3pcd);

// Forces unpartitioned storage access with third-party cookie blocking.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kNativeUnpartitionedStoragePermittedWhen3PCOff);

// Shows the option to disable the v8 optimizer for unfamiliar sites on the
// site settings page.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kBlockV8OptimizerOnUnfamiliarSitesSetting);

////////////////////////////////////////////////////////////
// Start of third-party cookie access heuristics features //
////////////////////////////////////////////////////////////

// The content module implements the third-party cookie (3PC or TPC) access
// heuristics described here:
// https://github.com/amaliev/3pcd-exemption-heuristics/blob/main/explainer.md
//
// At a high level, the heuristics are enabled/disabled by the
// kTpcdHeuristicsGrants Feature.
//
// The heuristics can be tweaked through the FeatureParams declared below. They
// affect when the heuristics apply and how long the temporary cookie access
// lasts.
//
// The heuristics grant third-party cookie access via calls to
// ContentBrowserClient::GrantCookieAccessDueToHeuristic(). Embedders should
// take these calls, kTpcdHeuristicsGrants, and kTpcdReadHeuristicsGrants into
// account in their implementation of
// ContentBrowserClient::IsFullCookieAccessAllowed().

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdReadHeuristicsGrantsName[];

// Enables writing and reading temporary storage access grants from 3PCD
// heuristics.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kTpcdHeuristicsGrants);

// Whether 3PCD heuristics grants should be considered to override cookie access
// behavior.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kTpcdReadHeuristicsGrants;

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdWriteRedirectHeuristicGrantsName[];
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdRedirectHeuristicRequireABAFlowName[];
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdRedirectHeuristicRequireCurrentInteractionName[];

// The duration of the storage access grant created when observing the Redirect
// With Current Interaction scenario. If set to zero duration, do not create a
// grant.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTpcdWriteRedirectHeuristicGrants;

// Whether to require an A-B-A flow (where the first party preceded the
// third-party redirect in the tab history) when applying the Redirect
// heuristic.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kTpcdRedirectHeuristicRequireABAFlow;

// Whether to require the third-party interaction to be in the current
// navigation when applying the Redirect heuristic.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool>
    kTpcdRedirectHeuristicRequireCurrentInteraction;

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdPopupHeuristicEnableForIframeInitiatorName[];
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdWritePopupCurrentInteractionHeuristicsGrantsName[];
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdWritePopupPastInteractionHeuristicsGrantsName[];
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdBackfillPopupHeuristicsGrantsName[];
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdPopupHeuristicDisableForAdTaggedPopupsName[];

enum class EnableForIframeTypes { kNone = 0, kFirstParty = 1, kAll = 2 };

// Whether to enable writing Popup heuristic grants when the popup is opened via
// an iframe initiator.

// * kNone: Ignore popups initiated from iframes.
// * kFirstPartyIframes: Only write grants for popups initiated from 1P iframes,
// or nested tree of all 1P iframes.
// * kAllIframes: Write grants for popups initiated from any frame.
constexpr base::FeatureParam<EnableForIframeTypes>::Option
    kEnableForIframeTypesOptions[] = {
        {EnableForIframeTypes::kNone, "none"},
        {EnableForIframeTypes::kFirstParty, "first-party"},
        {EnableForIframeTypes::kAll, "all"},
};
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<EnableForIframeTypes>
    kTpcdPopupHeuristicEnableForIframeInitiator;

// The duration of the storage access grant created when observing the Popup
// With Current Interaction scenario. If set to zero duration, do not create a
// grant.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupCurrentInteractionHeuristicsGrants;

// The duration of the storage access grant created when observing the Popup
// With Past Interaction scenario. If set to zero duration, do not create a
// grant.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupPastInteractionHeuristicsGrants;

// The lookback and duration of the storage access grants created when
// backfilling the Popup With Current Interaction scenario on onboarding to
// 3PCD. If set to zero duration, to not create backfill grants.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTpcdBackfillPopupHeuristicsGrants;

// Whether to disable writing Popup heuristic grants when the popup is opened
// via an ad-tagged frame.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool>
    kTpcdPopupHeuristicDisableForAdTaggedPopups;

//////////////////////////////////////////////////////////
// End of third-party cookie access heuristics features //
//////////////////////////////////////////////////////////

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kUseTestMetadataName[];

// TODO(crbug.com/415223384):
// `document.requestStorageAccess` is racy when permission has been overridden
// (e.g. via `test_driver.set_permission`). This is because the RFHI in the
// browser process may not be aware that the renderer has requested (and gotten)
// permission by the time StorageAccessHandle tries to bind mojo endpoints.
// This is used in the virtual test suite `force-allow-storage-access` to ensure
// no WPTs go stale while we wait on the less temporary fix in the task
// linked above.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kForceAllowStorageAccess);

}  // namespace features
}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_
