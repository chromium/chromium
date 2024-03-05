// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

ConsolidatedConsentScreenHandler::ConsolidatedConsentScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ConsolidatedConsentScreenHandler::~ConsolidatedConsentScreenHandler() = default;

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
  builder->Add("consolidatedConsentRecoveryOptInTitle",
               IDS_CONSOLIDATED_CONSENT_RECOVERY_OPT_IN_TITLE);
  builder->AddF("consolidatedConsentRecoveryOptIn",
                IDS_CONSOLIDATED_CONSENT_RECOVERY_OPT_IN,
                ui::GetChromeOSDeviceTypeResourceId());
  if (features::IsCrosPrivacyHubLocationEnabled()) {
    builder->Add("consolidatedConsentLocationOptInTitle",
                 IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_TITLE);
    builder->Add("consolidatedConsentLocationOptIn",
                 IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN);
    builder->Add("consolidatedConsentLocationOptInChild",
                 IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_CHILD);
    builder->Add("consolidatedConsenttLocationOptInLearnMoreLink",
                 IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE_LINK);
    builder->Add("consolidatedConsentFooter", IDS_CONSOLIDATED_CONSENT_FOOTER);
    builder->Add("consolidatedConsentFooterChild",
                 IDS_CONSOLIDATED_CONSENT_FOOTER_CHILD);
    builder->Add("consolidatedConsentLocationOptInLearnMore",
                 IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_LEARN_MORE);
    builder->Add(
        "consolidatedConsentLocationOptInLearnMoreChild",
        IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_LEARN_MORE_CHILD);

  } else {
    builder->Add("consolidatedConsentLocationOptInTitle",
                 IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_TITLE);
    builder->Add("consolidatedConsentLocationOptIn",
                 IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN);
    builder->Add("consolidatedConsentLocationOptInChild",
                 IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_CHILD);
    builder->Add("consolidatedConsenttLocationOptInLearnMoreLink",
                 IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_LEARN_MORE_LINK);
    builder->Add("consolidatedConsentFooter", IDS_CONSOLIDATED_CONSENT_FOOTER);
    builder->Add("consolidatedConsentFooterChild",
                 IDS_CONSOLIDATED_CONSENT_FOOTER_CHILD);
    builder->Add("consolidatedConsentLocationOptInLearnMore",
                 IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_LEARN_MORE);
    builder->Add("consolidatedConsentLocationOptInLearnMoreChild",
                 IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_LEARN_MORE_CHILD);
  }
  if (crosapi::browser_util::IsLacrosEnabled() &&
      features::IsOsSyncConsentRevampEnabled()) {
    builder->Add("consolidatedConsentUsageOptInLearnMore",
                 IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_OWNER_LACROS);
    builder->Add(
        "consolidatedConsentUsageOptInLearnMoreChild",
        IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_CHILD_OWNER_LACROS);
    builder->Add(
        "consolidatedConsentUsageOptInLearnMoreArcDisabled",
        IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_OWNER_LACROS);
    builder->Add(
        "consolidatedConsentUsageOptInLearnMoreArcDisabledChild",
        IDS_CONSOLIDATED_CONSENT_USAGE_OPT_IN_LEARN_MORE_ARC_DISABLED_CHILD_OWNER_LACROS);

  } else {
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
  }

  builder->Add("consolidatedConsentBackupOptInLearnMore",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE);
  builder->Add("consolidatedConsentBackupOptInLearnMoreChild",
               IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_LEARN_MORE_CHILD);
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
  builder->Add("consolidatedConsentPlayErrorTitle",
               IDS_CONSOLIDATED_CONSENT_PLAY_LOAD_ERROR_TITLE);
  builder->AddF("consolidatedConsentPlayErrorMessage",
                IDS_CONSOLIDATED_CONSENT_PLAY_LOAD_ERROR_MESSAGE,
                ui::GetChromeOSDeviceTypeResourceId());
  builder->Add("consolidatedConsentDone", IDS_CONSOLIDATED_CONSENT_DONE);
  builder->Add("consolidatedConsentRetry", IDS_CONSOLIDATED_CONSENT_TRY_AGAIN);
  builder->Add("consolidatedConsentOK", IDS_CONSOLIDATED_CONSENT_OK);
  builder->Add("consolidatedConsentGoogleEulaTitle",
               IDS_CONSOLIDATED_CONSENT_GOOGLE_EULA_TITLE);
  builder->Add("consolidatedConsentCrosEulaTitle",
               IDS_CONSOLIDATED_CONSENT_CROS_EULA_TITLE);
  builder->Add("consolidatedConsentArcTermsTitle",
               IDS_CONSOLIDATED_CONSENT_ARC_TITLE);
  builder->Add("consolidatedConsentPrivacyTitle",
               IDS_CONSOLIDATED_CONSENT_PRIVACY_POLICY_TITLE);
}

void ConsolidatedConsentScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

void ConsolidatedConsentScreenHandler::SetUsageMode(bool enabled,
                                                    bool managed) {
  CallExternalAPI("setUsageMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetBackupMode(bool enabled,
                                                     bool managed) {
  CallExternalAPI("setBackupMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetLocationMode(bool enabled,
                                                       bool managed) {
  CallExternalAPI("setLocationMode", enabled, managed);
}

void ConsolidatedConsentScreenHandler::SetUsageOptinHidden(bool hidden) {
  CallExternalAPI("setUsageOptinHidden", hidden);
}

base::WeakPtr<ConsolidatedConsentScreenView>
ConsolidatedConsentScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
