// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/card_data_helper.h"

#include <algorithm>
#include <list>
#include <map>
#include <optional>
#include <vector>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

base::Value::Dict CardDataToValue(int header_id,
                                  int subheader_id,
                                  safety_hub::SafetyHubCardState card_state) {
  base::Value::Dict card_info;

  card_info.Set(safety_hub::kCardHeaderKey,
                l10n_util::GetStringUTF16(header_id));
  card_info.Set(safety_hub::kCardSubheaderKey,
                l10n_util::GetStringUTF16(subheader_id));
  card_info.Set(safety_hub::kCardStateKey, static_cast<int>(card_state));

  return card_info;
}
}  // namespace

namespace safety_hub {

base::Value::Dict GetVersionCardData() {
  base::Value::Dict result;
  switch (g_browser_process->GetBuildState()->update_type()) {
    case BuildState::UpdateType::kNone:
      result.Set(kCardHeaderKey,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_UPDATED));
      result.Set(kCardSubheaderKey,
                 VersionUI::GetAnnotatedVersionStringForUi());
      result.Set(kCardStateKey, static_cast<int>(SafetyHubCardState::kSafe));
      break;
    case BuildState::UpdateType::kNormalUpdate:
    // kEnterpriseRollback and kChannelSwitchRollback are fairly rare state,
    // they will be handled same as there is waiting updates.
    case BuildState::UpdateType::kEnterpriseRollback:
    case BuildState::UpdateType::kChannelSwitchRollback:
      result = CardDataToValue(
          IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_RESTART,
          IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_SUBHEADER_RESTART,
          SafetyHubCardState::kWarning);
  }
  return result;
}

base::Value::Dict GetPasswordCardData(Profile* profile) {
  PasswordStatusCheckService* service =
      PasswordStatusCheckServiceFactory::GetForProfile(profile);
  CHECK(service);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool signed_in = identity_manager && identity_manager->HasPrimaryAccount(
                                           signin::ConsentLevel::kSignin);

  return service->GetPasswordCardData(signed_in);
}

base::Value::Dict GetSafeBrowsingCardData(Profile* profile) {
  SafeBrowsingState state =
      SafetyHubSafeBrowsingResult::GetState(profile->GetPrefs());
  base::Value::Dict sb_card_info;

  switch (state) {
    case SafeBrowsingState::kEnabledEnhanced:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_SUBHEADER,
                          SafetyHubCardState::kSafe);
      break;
    case SafeBrowsingState::kEnabledStandard:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_SUBHEADER,
                          SafetyHubCardState::kSafe);
      break;
    case SafeBrowsingState::kDisabledByAdmin:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_MANAGED_SUBHEADER,
                          SafetyHubCardState::kInfo);
      break;
    case SafeBrowsingState::kDisabledByExtension:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_EXTENSION_SUBHEADER,
                          SafetyHubCardState::kInfo);
      break;
    default:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_USER_SUBHEADER,
                          SafetyHubCardState::kWarning);
  }
  return sb_card_info;
}

SafetyHubCardState GetOverallState(Profile* profile) {
  // If there are any modules that need to be reviewed, the overall state is
  // "warning".
  UnusedSitePermissionsService* usp_service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_usp_result =
      usp_service->GetCachedResult();
  if (opt_usp_result.has_value()) {
    auto* result =
        static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
            opt_usp_result.value().get());
    if (!result->GetRevokedOrigins().empty()) {
      return SafetyHubCardState::kWarning;
    }
  }

  NotificationPermissionsReviewService* npr_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_npr_result =
      npr_service->GetCachedResult();
  if (opt_npr_result.has_value()) {
    auto* result = static_cast<
        NotificationPermissionsReviewService::NotificationPermissionsResult*>(
        opt_npr_result.value().get());
    if (!result->GetSortedNotificationPermissions().empty()) {
      return SafetyHubCardState::kWarning;
    }
  }

  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_ext_result =
      SafetyHubExtensionsResult::GetResult(profile, true);
  if (opt_ext_result.has_value()) {
    auto* result =
        static_cast<SafetyHubExtensionsResult*>(opt_ext_result.value().get());
    if (result->GetNumTriggeringExtensions() > 0) {
      return SafetyHubCardState::kWarning;
    }
  }

  // Get the version card data for all remaining modules.
  std::vector<base::Value::Dict> cards;
  cards.push_back(GetVersionCardData());
  cards.push_back(GetPasswordCardData(profile));
  cards.push_back(GetSafeBrowsingCardData(profile));

  // Return the lowest value, which coincides with the "worst" global state.
  SafetyHubCardState min_value = SafetyHubCardState::kMaxValue;

  for (const auto& card : cards) {
    SafetyHubCardState current =
        static_cast<SafetyHubCardState>(card.FindInt(kCardStateKey).value());
    if (current < min_value) {
      min_value = current;
    }
  }

  return min_value;
}

}  // namespace safety_hub
