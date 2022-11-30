// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/cert_report_helper.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Certificate reports are only sent from official builds, but this flag can be
// set by tests.
static bool g_is_fake_official_build_for_cert_report_testing = false;

PrefService* GetPrefs(content::WebContents* web_contents) {
  return user_prefs::UserPrefs::Get(web_contents->GetBrowserContext());
}

}  // namespace

// Constants for the HTTPSErrorReporter Finch experiment
const char CertReportHelper::kFinchExperimentName[] = "ReportCertificateErrors";
const char CertReportHelper::kFinchGroupShowPossiblySend[] =
    "ShowAndPossiblySend";
const char CertReportHelper::kFinchGroupDontShowDontSend[] =
    "DontShowAndDontSend";
const char CertReportHelper::kFinchParamName[] = "sendingThreshold";

CertReportHelper::CertReportHelper(
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    content::WebContents* web_contents,
    const GURL& request_url,
    const net::SSLInfo& ssl_info,
    CertificateErrorReport::InterstitialReason interstitial_reason,
    bool overridable,
    const base::Time& interstitial_time,
    bool can_show_enhanced_protection_message,
    security_interstitials::MetricsHelper* metrics_helper)
    : ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      web_contents_(web_contents),
      request_url_(request_url),
      ssl_info_(ssl_info),
      interstitial_reason_(interstitial_reason),
      overridable_(overridable),
      interstitial_time_(interstitial_time),
      can_show_enhanced_protection_message_(
          can_show_enhanced_protection_message),
      metrics_helper_(metrics_helper) {}

CertReportHelper::~CertReportHelper() = default;

// static
void CertReportHelper::SetFakeOfficialBuildForTesting() {
  g_is_fake_official_build_for_cert_report_testing = true;
}

void CertReportHelper::PopulateExtendedReportingOption(
    base::Value::Dict& load_time_data) {
  // Only show the checkbox if not off-the-record and if this client is
  // part of the respective Finch group, and the feature is not disabled
  // by policy.
  const bool show = ShouldShowCertificateReporterCheckbox() &&
                    !ShouldShowEnhancedProtectionMessage();

  load_time_data.Set(security_interstitials::kDisplayCheckBox, show);
  if (!show)
    return;

  load_time_data.Set(
      security_interstitials::kBoxChecked,
      safe_browsing::IsExtendedReportingEnabled(*GetPrefs(web_contents_)));

  load_time_data.Set(
      security_interstitials::kOptInLink,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
}

void CertReportHelper::PopulateEnhancedProtectionMessage(
    base::Value::Dict& load_time_data) {
  const bool show = ShouldShowEnhancedProtectionMessage();

  load_time_data.Set(security_interstitials::kDisplayEnhancedProtectionMessage,
                     show);

  if (!show)
    return;

  if (metrics_helper_) {
    metrics_helper_->RecordUserInteraction(
        security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION);
  }

  load_time_data.Set(
      security_interstitials::kEnhancedProtectionMessage,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
}

void CertReportHelper::SetSSLCertReporterForTesting(
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  ssl_cert_reporter_ = std::move(ssl_cert_reporter);
}

void CertReportHelper::HandleReportingCommands(
    security_interstitials::SecurityInterstitialCommand command,
    PrefService* pref_service) {
  switch (command) {
    case security_interstitials::CMD_DO_REPORT:
      safe_browsing::SetExtendedReportingPrefAndMetric(
          pref_service, true, /* value */
          safe_browsing::SBER_OPTIN_SITE_SECURITY_INTERSTITIAL);
      break;
    case security_interstitials::CMD_DONT_REPORT:
      safe_browsing::SetExtendedReportingPrefAndMetric(
          pref_service, false, /* value */
          safe_browsing::SBER_OPTIN_SITE_SECURITY_INTERSTITIAL);
      break;
    case security_interstitials::CMD_PROCEED:
      user_action_ = CertificateErrorReport::USER_PROCEEDED;
      break;
    case security_interstitials::CMD_DONT_PROCEED:
      user_action_ = CertificateErrorReport::USER_DID_NOT_PROCEED;
      break;
    default:
      // Other commands can be ignored.
      break;
  }
}

void CertReportHelper::FinishCertCollection() {
  if (!ShouldShowCertificateReporterCheckbox())
    return;

  if (!safe_browsing::IsExtendedReportingEnabled(*GetPrefs(web_contents_)))
    return;

  if (metrics_helper_) {
    metrics_helper_->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }

  if (!ShouldReportCertificateError())
    return;

  std::string serialized_report;
  CertificateErrorReport report(request_url_.host(), ssl_info_);

  if (client_details_callback_)
    client_details_callback_.Run(&report);

  report.SetInterstitialInfo(
      interstitial_reason_, user_action_,
      overridable_ ? CertificateErrorReport::INTERSTITIAL_OVERRIDABLE
                   : CertificateErrorReport::INTERSTITIAL_NOT_OVERRIDABLE,
      interstitial_time_);

  if (!report.Serialize(&serialized_report)) {
    LOG(ERROR) << "Failed to serialize certificate report.";
    return;
  }

  ssl_cert_reporter_->ReportInvalidCertificateChain(serialized_report);
}

bool CertReportHelper::ShouldShowCertificateReporterCheckbox() {
  // Only show the checkbox iff the user is part of the respective Finch group
  // and the window is not incognito and the feature is not disabled by policy
  // and enhanced protection is off.
  const bool in_incognito =
      web_contents_->GetBrowserContext()->IsOffTheRecord();
  const PrefService* pref_service = GetPrefs(web_contents_);
  bool can_show_checkbox =
      safe_browsing::IsExtendedReportingOptInAllowed(*pref_service) &&
      !safe_browsing::IsExtendedReportingPolicyManaged(*pref_service);
  bool is_enhanced_protection_enabled =
      safe_browsing::IsEnhancedProtectionEnabled(*pref_service);

  return base::FieldTrialList::FindFullName(kFinchExperimentName) ==
             kFinchGroupShowPossiblySend &&
         !in_incognito && can_show_checkbox && !is_enhanced_protection_enabled;
}

bool CertReportHelper::ShouldShowEnhancedProtectionMessage() {
  // Only show the enhanced protection message if all the following are true:
  // |can_show_enhanced_protection_message_| is set to true AND
  // the window is not incognito AND
  // Safe Browsing is not managed by policy AND
  // the user is not already in enhanced protection mode.
  if (!can_show_enhanced_protection_message_) {
    return false;
  }

  const bool in_incognito =
      web_contents_->GetBrowserContext()->IsOffTheRecord();
  const PrefService* pref_service = GetPrefs(web_contents_);
  bool is_enhanced_protection_enabled =
      safe_browsing::IsEnhancedProtectionEnabled(*pref_service);
  bool is_safe_browsing_managed =
      safe_browsing::IsSafeBrowsingPolicyManaged(*pref_service);

  if (in_incognito) {
    return false;
  }
  if (is_enhanced_protection_enabled) {
    return false;
  }
  if (is_safe_browsing_managed) {
    return false;
  }
  return true;
}

bool CertReportHelper::ShouldReportCertificateError() {
  DCHECK(ShouldShowCertificateReporterCheckbox());

  bool is_official_build = g_is_fake_official_build_for_cert_report_testing;
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_official_build = true;
#endif

  if (!is_official_build)
    return false;

  // Even in case the checkbox was shown, we don't send error reports
  // for all of these users. Check the Finch configuration for a sending
  // threshold and only send reports in case the threshold isn't exceeded.
  const std::string param =
      variations::GetVariationParamValue(kFinchExperimentName, kFinchParamName);
  if (!param.empty()) {
    double sendingThreshold;
    if (base::StringToDouble(param, &sendingThreshold)) {
      if (sendingThreshold >= 0.0 && sendingThreshold <= 1.0)
        return base::RandDouble() <= sendingThreshold;
    }
  }
  return false;
}
