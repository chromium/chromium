// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

ConsolidatedConsentScreenView::ScreenConfig::ScreenConfig() = default;

ConsolidatedConsentScreenView::ScreenConfig::~ScreenConfig() = default;

constexpr StaticOobeScreenId ConsolidatedConsentScreenView::kScreenId;

ConsolidatedConsentScreenHandler::ConsolidatedConsentScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.ConsolidatedConsentScreen.userActed");
}

ConsolidatedConsentScreenHandler::~ConsolidatedConsentScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void ConsolidatedConsentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("consolidatedConsentHeader", IDS_CONSOLIDATED_CONSENT_HEADER);
  builder->Add("consolidatedConsentHeaderChild",
               IDS_CONSOLIDATED_CONSENT_HEADER_CHILD);
  builder->Add("consolidatedConsentSubheader",
               IDS_CONSOLIDATED_CONSENT_SUBHEADER);
  builder->Add("consolidatedConsentTermsDescriptionTitle",
               IDS_CONSOLIDATED_CONSENT_TOS_TITLE);
  builder->Add("consolidatedConsentTermsDescription",
               IDS_CONSOLIDATED_CONSENT_TOS);
  builder->Add("consolidatedConsentTermsDescriptionArcDisabled",
               IDS_CONSOLIDATED_CONSENT_TOS_ARC_DISABLED);
  builder->Add("consolidatedConsentUsageOptInTitle",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_TITLE);
  builder->Add("consolidatedConsentUsageOptIn",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN);
  builder->Add("consolidatedConsentUsageOptInChild",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_CHILD);
  builder->Add("consolidatedConsentUsageOptInArcDisabled",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_ARC_DISABLED);
  builder->Add("consolidatedConsentBackupOptInTitle",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_TITLE);
  builder->Add("consolidatedConsentBackupOptIn",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN);
  builder->Add("consolidatedConsentBackupOptInChild",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_CHILD);
  builder->Add("consolidatedConsentLocationOptInTitle",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_TITLE);
  builder->Add("consolidatedConsentLocationOptIn",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN);
  builder->Add("consolidatedConsentLocationOptInChild",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_CHILD);
  builder->Add("consolidatedConsentFooter", IDS_CONSOLIDATED_CONSENT_FOOTER);
  builder->Add("consolidatedConsentFooterChild",
               IDS_CONSOLIDATED_CONSENT_FOOTER_CHILD);
  builder->Add("consolidatedConsentUsageOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentUsageOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_CHILD);
  builder->Add("consolidatedConsentUsageOptInLearnMoreArcDisabled",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED);
  builder->Add(
      "consolidatedConsentUsageOptInLearnMoreArcDisabledChild",
      IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_CHILD);
  builder->Add("consolidatedConsentBackupOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentBackupOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE_CHILD);
  builder->Add("consolidatedConsentLocationOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentLocationOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE_CHILD);
  builder->Add("consolidatedConsentFooterLearnMore",
               IDS_CONSOLIDATED_FOOTER_LEARN_MORE);
  builder->Add("consolidatedConsentLearnMore",
               IDS_CONSOLIDATED_CONSENT_LEARN_MORE);
  builder->Add("consolidatedConsentAcceptAndContinue",
               IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  builder->Add("consolidatedConsentLoading", IDS_CONSOLIDATED_LOADING);
  builder->Add("consolidatedConsentErrorTitle",
               IDS_OOBE_GENERIC_FATAL_ERROR_TITLE);
  builder->Add("consolidatedConsentErrorMessage",
               IDS_CONSOLIDATED_TERMS_LOAD_ERROR);
  builder->Add("consolidatedConsentRetry", IDS_CONSOLIDATED_TERMS_RETRY);
  builder->Add("consolidatedConsentOK", IDS_CONSOLIDATED_OK);
  builder->Add("consolidatedConsentEulaTermsTitle",
               IDS_CONSOLIDATED_EULA_TERMS_TITLE);
  builder->Add("consolidatedConsentAdditionalTermsTitle",
               IDS_CONSOLIDATED_ADDITIONAL_TERMS_TITLE);
  builder->Add("consolidatedConsentArcTermsTitle",
               IDS_CONSOLIDATED_ARC_TERMS_TITLE);
  builder->Add("consolidatedConsentPrivacyTermsTitle",
               IDS_CONSOLIDATED_PRIVACY_POLICY_TERMS_TITLE);
}

void ConsolidatedConsentScreenHandler::Initialize() {}

void ConsolidatedConsentScreenHandler::Show(const ScreenConfig& config) {
  base::DictionaryValue data;
  data.SetBoolean("isArcEnabled", config.is_arc_enabled);
  data.SetBoolean("isDemo", config.is_demo);
  data.SetBoolean("isChildAccount", config.is_child_account);
  data.SetString("countryCode", config.country_code);
  data.SetString("eulaUrl", config.eula_url);
  data.SetString("additionalTosUrl", config.additional_tos_url);
  ShowScreenWithData(kScreenId, &data);
}

void ConsolidatedConsentScreenHandler::Bind(ConsolidatedConsentScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ConsolidatedConsentScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ConsolidatedConsentScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();

  AddCallback("ToSAccept", &ConsolidatedConsentScreenHandler::HandleAccept);
}

void ConsolidatedConsentScreenHandler::HandleAccept(
    bool enable_stats_usage,
    bool enable_backup_restore,
    bool enable_location_services,
    const std::string& tos_content) {
  screen_->OnAccept(enable_stats_usage, enable_backup_restore,
                    enable_location_services, tos_content);
}

void ConsolidatedConsentScreenHandler::SetUsageMode(bool enabled,
                                                    bool managed) {
  CallJS("login.ConsolidatedConsentScreen.SetUsageMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetBackupMode(bool enabled,
                                                     bool managed) {
  CallJS("login.ConsolidatedConsentScreen.setBackupMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetLocationMode(bool enabled,
                                                       bool managed) {
  CallJS("login.ConsolidatedConsentScreen.setLocationMode", enabled, managed);
}

}  // namespace chromeos
