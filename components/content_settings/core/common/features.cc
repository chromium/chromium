// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content_settings {

// Enables auto dark feature in theme settings.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kDarkenWebsitesCheckboxInThemesSetting,
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut{
    &kDarkenWebsitesCheckboxInThemesSetting, "opt_out", true};
#endif  // BUILDFLAG(IS_ANDROID)

namespace features {

// Enables unused site permission module in Safety Check.
BASE_FEATURE(kSafetyCheckUnusedSitePermissions,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else   // BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kActiveContentSettingExpiry, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-no-delay-for-testing", false};

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsWithDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-with-delay-for-testing", false};

BASE_FEATURE(kApproximateGeolocationPermission,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kApproximateGeolocationPermissionPromptArm(
    &features::kApproximateGeolocationPermission,
    "prompt_arm",
    0);

BASE_FEATURE(kUserBypassUI, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kUserBypassUIExceptionExpiration{
    &kUserBypassUI, "expiration", base::Days(90)};

const base::FeatureParam<int> kUserBypassUIReloadCount{&kUserBypassUI,
                                                       "reload-count", 2};

const base::FeatureParam<base::TimeDelta> kUserBypassUIReloadTime{
    &kUserBypassUI, "reload-time", base::Seconds(30)};

const base::FeatureParam<base::TimeDelta> kUserBypassUIReloadBubbleTimeout{
    &kUserBypassUI, "reload-bubble-timeout", base::Seconds(5)};

BASE_FEATURE(kUserBypassFeedback, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLeftHandSideActivityIndicators, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kCrosSystemLevelPermissionBlockedWarnings,
             "CrosBlockWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kTrackingProtection3pcd, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNativeUnpartitionedStoragePermittedWhen3PCOff,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBlockV8OptimizerOnUnfamiliarSitesSetting,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTpcdReadHeuristicsGrantsName[] = "TpcdReadHeuristicsGrants";

const char kTpcdWriteRedirectHeuristicGrantsName[] =
    "TpcdWriteRedirectHeuristicGrants";
const char kTpcdRedirectHeuristicRequireABAFlowName[] =
    "TpcdRedirectHeuristicRequireABAFlow";
const char kTpcdRedirectHeuristicRequireCurrentInteractionName[] =
    "TpcdRedirectHeuristicRequireCurrentInteraction";

const char kTpcdWritePopupCurrentInteractionHeuristicsGrantsName[] =
    "TpcdWritePopupCurrentInteractionHeuristicsGrants";
const char kTpcdWritePopupPastInteractionHeuristicsGrantsName[] =
    "TpcdWritePopupPastInteractionHeuristicsGrants";
const char kTpcdBackfillPopupHeuristicsGrantsName[] =
    "TpcdBackfillPopupHeuristicsGrants";
const char kTpcdPopupHeuristicDisableForAdTaggedPopupsName[] =
    "TpcdPopupHeuristicDisableForAdTaggedPopups";
const char kTpcdPopupHeuristicEnableForIframeInitiatorName[] =
    "TpcdPopupHeuristicEnableForIframeInitiator";

BASE_FEATURE(kTpcdHeuristicsGrants, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kTpcdReadHeuristicsGrants{
    &kTpcdHeuristicsGrants, kTpcdReadHeuristicsGrantsName, true};

const base::FeatureParam<base::TimeDelta> kTpcdWriteRedirectHeuristicGrants{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdWriteRedirectHeuristicGrantsName, base::Minutes(15)};

const base::FeatureParam<bool> kTpcdRedirectHeuristicRequireABAFlow{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdRedirectHeuristicRequireABAFlowName, true};

const base::FeatureParam<bool> kTpcdRedirectHeuristicRequireCurrentInteraction{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdRedirectHeuristicRequireCurrentInteractionName, true};

const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupCurrentInteractionHeuristicsGrants{
        &content_settings::features::kTpcdHeuristicsGrants,
        kTpcdWritePopupCurrentInteractionHeuristicsGrantsName, base::Days(30)};

const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupPastInteractionHeuristicsGrants{
        &content_settings::features::kTpcdHeuristicsGrants,
        kTpcdWritePopupPastInteractionHeuristicsGrantsName, base::TimeDelta()};

const base::FeatureParam<base::TimeDelta> kTpcdBackfillPopupHeuristicsGrants{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdBackfillPopupHeuristicsGrantsName, base::Days(30)};

const base::FeatureParam<bool> kTpcdPopupHeuristicDisableForAdTaggedPopups{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdPopupHeuristicDisableForAdTaggedPopupsName, false};

const base::FeatureParam<EnableForIframeTypes>
    kTpcdPopupHeuristicEnableForIframeInitiator{
        &content_settings::features::kTpcdHeuristicsGrants,
        kTpcdPopupHeuristicEnableForIframeInitiatorName,
        EnableForIframeTypes::kAll, &kEnableForIframeTypesOptions};

BASE_FEATURE(kForceAllowStorageAccess, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace content_settings
