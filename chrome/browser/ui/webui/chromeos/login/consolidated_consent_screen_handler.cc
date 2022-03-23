// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
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
  builder->Add("consolidatedConsentHeaderManaged",
               IDS_CONSOLIDATED_CONSENT_HEADER_MANAGED);
  builder->Add("consolidatedConsentSubheader",
               IDS_CONSOLIDATED_CONSENT_SUBHEADER);
  builder->Add("consolidatedConsentSubheaderArcDisabled",
               IDS_CONSOLIDATED_CONSENT_SUBHEADER_ARC_DISABLED);
  builder->Add("consolidatedConsentTermsDescriptionTitle",
               IDS_CONSOLIDATED_CONSENT_TERMS_TITLE);
  builder->Add("consolidatedConsentTermsDescription",
               IDS_CONSOLIDATED_CONSENT_TERMS);
  builder->Add("consolidatedConsentTermsDescriptionArcDisabled",
               IDS_CONSOLIDATED_CONSENT_TERMS_ARC_DISABLED);
  builder->Add("consolidatedConsentUsageOptInTitle",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_TITLE);
  builder->Add("consolidatedConsentUsageOptIn",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN);
  builder->Add("consolidatedConsentUsageOptInOwner",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_OWNER);
  builder->Add("consolidatedConsentUsageOptInChild",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_CHILD);
  builder->Add("consolidatedConsentUsageOptInChildOwner",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_CHILD_OWNER);
  builder->Add("consolidatedConsentUsageOptInArcDisabled",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_ARC_DISABLED);
  builder->Add("consolidatedConsentUsageOptInArcDisabledOwner",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_ARC_DISABLED_OWNER);
  builder->Add("consolidatedConsentUsageOptInLearnMoreLink",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_LINK);
  builder->Add("consolidatedConsentBackupOptInTitle",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_TITLE);
  builder->Add("consolidatedConsentBackupOptIn",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN);
  builder->Add("consolidatedConsentBackupOptInChild",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_CHILD);
  builder->Add("consolidatedConsenttBackupOptInLearnMoreLink",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE_LINK);
  builder->Add("consolidatedConsentLocationOptInTitle",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_TITLE);
  builder->Add("consolidatedConsentLocationOptIn",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN);
  builder->Add("consolidatedConsentLocationOptInChild",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_CHILD);
  builder->Add("consolidatedConsenttLocationOptInLearnMoreLink",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE_LINK);
  builder->Add("consolidatedConsentFooter", IDS_CONSOLIDATED_CONSENT_FOOTER);
  builder->Add("consolidatedConsentFooterChild",
               IDS_CONSOLIDATED_CONSENT_FOOTER_CHILD);
  builder->Add("consolidatedConsentUsageOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentUsageOptInLearnMoreOwner",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_OWNER);
  builder->Add("consolidatedConsentUsageOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_CHILD);
  builder->Add("consolidatedConsentUsageOptInLearnMoreChildOwner",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_CHILD_OWNER);
  builder->Add("consolidatedConsentUsageOptInLearnMoreArcDisabled",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED);
  builder->Add(
      "consolidatedConsentUsageOptInLearnMoreArcDisabledOwner",
      IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_OWNER);
  builder->Add(
      "consolidatedConsentUsageOptInLearnMoreArcDisabledChild",
      IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_CHILD);
  builder->Add(
      "consolidatedConsentUsageOptInLearnMoreArcDisabledChildOwner",
      IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_CHILD_OWNER);
  builder->Add("consolidatedConsentBackupOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentBackupOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE_CHILD);
  builder->Add("consolidatedConsentLocationOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentLocationOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE_CHILD);
  builder->Add("consolidatedConsentFooterLearnMore",
               IDS_CONSOLIDATED_CONSENT_FOOTER_LEARN_MORE);
  builder->Add("consolidatedConsentLearnMore",
               IDS_CONSOLIDATED_CONSENT_LEARN_MORE);
  builder->Add("consolidatedConsentAcceptAndContinue",
               IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  builder->Add("consolidatedConsentLoading", IDS_CONSOLIDATED_CONSENT_LOADING);
  builder->Add("consolidatedConsentErrorTitle",
               IDS_OOBE_GENERIC_FATAL_ERROR_TITLE);
  builder->Add("consolidatedConsentErrorMessage",
               IDS_CONSOLIDATED_CONSENT_TERMS_LOAD_ERROR);
  builder->Add("consolidatedConsentRetry", IDS_CONSOLIDATED_CONSENT_RETRY);
  builder->Add("consolidatedConsentOK", IDS_CONSOLIDATED_CONSENT_OK);
  builder->Add("consolidatedConsentGoogleEulaTitle",
               IDS_CONSOLIDATED_CONSENT_GOOGLE_EULA_TITLE);
  builder->Add("consolidatedConsentCrosEulaTitle",
               IDS_CONSOLIDATED_CONSENT_CROS_EULA_TITLE);
  builder->Add("consolidatedConsentArcTermsTitle",
               IDS_CONSOLIDATED_CONSENT_ARC_TITLE);
  builder->Add("consolidatedConsentPrivacyTitle",
               IDS_CONSOLIDATED_CONSENT_PRIVACY_POLICY_TITLE);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kArcTosHostForTests)) {
    builder->Add("consolidatedConsentArcTosHostNameForTesting",
                 base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                     switches::kArcTosHostForTests));
  }
}

void ConsolidatedConsentScreenHandler::Initialize() {}

void ConsolidatedConsentScreenHandler::Show(const ScreenConfig& config) {
  base::DictionaryValue data;
  // If ARC is enabled, show the ARC ToS and the related opt-ins.
  data.SetBoolKey("isArcEnabled", config.is_arc_enabled);
  // In demo mode, don't show any opt-ins related to ARC and allow showing the
  // offline ARC ToS if the online version failed to load.
  data.SetBoolKey("isDemo", config.is_demo);
  // Child accounts have alternative strings for the opt-ins.
  data.SetBoolKey("isChildAccount", config.is_child_account);
  // Managed account will not be shown any terms of service, and the title
  // string will be updated.
  data.SetBoolKey("isEnterpriseManagedAccount",
                  config.is_enterprise_managed_account);
  // Country code is needed to load the ARC ToS.
  data.SetStringKey("countryCode", config.country_code);
  // URL for EULA, the URL should include the locale.
  data.SetStringKey("googleEulaUrl", config.google_eula_url);
  // URL for Chrome and ChromeOS additional terms of service, the URL should
  // include the locale.
  data.SetStringKey("crosEulaUrl", config.cros_eula_url);
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
  if (screen_) {
    screen_->OnAccept(enable_stats_usage, enable_backup_restore,
                      enable_location_services, tos_content);
  }
}

void ConsolidatedConsentScreenHandler::SetUsageMode(bool enabled,
                                                    bool managed) {
  CallJS("login.ConsolidatedConsentScreen.setUsageMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetBackupMode(bool enabled,
                                                     bool managed) {
  CallJS("login.ConsolidatedConsentScreen.setBackupMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetLocationMode(bool enabled,
                                                       bool managed) {
  CallJS("login.ConsolidatedConsentScreen.setLocationMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetIsDeviceOwner(bool is_owner) {
  CallJS("login.ConsolidatedConsentScreen.setIsDeviceOwner", is_owner);
}

void ConsolidatedConsentScreenHandler::HideUsageOptin() {
  CallJS("login.ConsolidatedConsentScreen.setUsageOptinHidden");
}
}  // namespace chromeos
