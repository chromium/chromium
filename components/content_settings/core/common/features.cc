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
             "DarkenWebsitesCheckboxInThemesSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut{
    &kDarkenWebsitesCheckboxInThemesSetting, "opt_out", true};
#endif  // BUILDFLAG(IS_ANDROID)

namespace features {

// Enables unused site permission module in Safety Check.
BASE_FEATURE(kSafetyCheckUnusedSitePermissions,
             "SafetyCheckUnusedSitePermissions",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else   // BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions,
             "SafetyCheckUnusedSitePermissionsForSupportedChooserPermissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kActiveContentSettingExpiry,
             "ActiveContentSettingExpiry",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRepeatedUpdateInterval{
        &kSafetyCheckUnusedSitePermissions,
        "unused-site-repeated-update-interval", base::Days(1)};

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-no-delay-for-testing", false};

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsWithDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-with-delay-for-testing", false};

const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRevocationThreshold{
        &kSafetyCheckUnusedSitePermissions,
        "unused-site-permissions-revocation-threshold", base::Days(60)};

const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold{
        &kSafetyCheckUnusedSitePermissions,
        "unused-site-permissions-revocation-cleanup-threshold", base::Days(30)};

BASE_FEATURE(kUserBypassUI, "UserBypassUI", base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kUserBypassUIExceptionExpiration{
    &kUserBypassUI, "expiration", base::Days(90)};

const base::FeatureParam<int> kUserBypassUIReloadCount{&kUserBypassUI,
                                                       "reload-count", 2};

const base::FeatureParam<base::TimeDelta> kUserBypassUIReloadTime{
    &kUserBypassUI, "reload-time", base::Seconds(30)};

const base::FeatureParam<base::TimeDelta> kUserBypassUIReloadBubbleTimeout{
    &kUserBypassUI, "reload-bubble-timeout", base::Seconds(5)};

BASE_FEATURE(kUserBypassFeedback,
             "UserBypassFeedback",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLeftHandSideActivityIndicators,
             "LeftHandSideActivityIndicators",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kCrosSystemLevelPermissionBlockedWarnings,
             "CrosBlockWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kTrackingProtection3pcd,
             "TrackingProtection3pcd",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNativeUnpartitionedStoragePermittedWhen3PCOff,
             "NativeUnpartitionedStoragePermittedWhen3PCOff",
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

BASE_FEATURE(kTpcdHeuristicsGrants,
             "TpcdHeuristicsGrants",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kContentSettingsPartitioning,
             "ContentSettingsPartitioning",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceAllowStorageAccess,
             "ForceAllowStorageAccess",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace content_settings
