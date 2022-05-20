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

ConsolidatedConsentScreenHandler::ConsolidatedConsentScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.ConsolidatedConsentScreen.userActed");
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
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_OWNER);
  builder->Add("consolidatedConsentUsageOptInChild",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_CHILD_OWNER);
  builder->Add("consolidatedConsentUsageOptInArcDisabled",
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
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_OWNER);
  builder->Add("consolidatedConsentUsageOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_CHILD_OWNER);
  builder->Add(
      "consolidatedConsentUsageOptInLearnMoreArcDisabled",
      IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_OWNER);
  builder->Add(
      "consolidatedConsentUsageOptInLearnMoreArcDisabledChild",
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

void ConsolidatedConsentScreenHandler::InitializeDeprecated() {}

void ConsolidatedConsentScreenHandler::Show(const ScreenConfig& config) {
  base::Value::Dict data;
  // If ARC is enabled, show the ARC ToS and the related opt-ins.
  data.Set("isArcEnabled", config.is_arc_enabled);
  // In demo mode, don't show any opt-ins related to ARC and allow showing the
  // offline ARC ToS if the online version failed to load.
  data.Set("isDemo", config.is_demo);
  // Child accounts have alternative strings for the opt-ins.
  data.Set("isChildAccount", config.is_child_account);
  // If the user is affiliated with the device management domain, ToS should be
  // hidden.
  data.Set("isTosHidden", config.is_tos_hidden);
  // Country code is needed to load the ARC ToS.
  data.Set("countryCode", config.country_code);
  // URL for EULA, the URL should include the locale.
  data.Set("googleEulaUrl", config.google_eula_url);
  // URL for Chrome and ChromeOS additional terms of service, the URL should
  // include the locale.
  data.Set("crosEulaUrl", config.cros_eula_url);
  ShowInWebUI(std::move(data));
}

void ConsolidatedConsentScreenHandler::Bind(ConsolidatedConsentScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
}

void ConsolidatedConsentScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
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

void ConsolidatedConsentScreenHandler::SetUsageOptinOptinHidden(bool hidden) {
  CallJS("login.ConsolidatedConsentScreen.setUsageOptinHidden", hidden);
}
}  // namespace chromeos
