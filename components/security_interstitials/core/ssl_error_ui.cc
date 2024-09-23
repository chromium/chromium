// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/ssl_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "build/build_config.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {
namespace {

// Path to the relevant help center page. Used if |support_url_| is invalid.
const char kHelpPath[] = "answer/6098869";

bool IsMasked(int options, SSLErrorOptionsMask mask) {
  return ((options & mask) != 0);
}

}  // namespace

SSLErrorUI::SSLErrorUI(const GURL& request_url,
                       int cert_error,
                       const net::SSLInfo& ssl_info,
                       int display_options,
                       const base::Time& time_triggered,
                       const GURL& support_url,
                       ControllerClient* controller)
    : request_url_(request_url),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      time_triggered_(time_triggered),
      support_url_(support_url),
      requested_strict_enforcement_(
          IsMasked(display_options, STRICT_ENFORCEMENT)),
      soft_override_enabled_(IsMasked(display_options, SOFT_OVERRIDE_ENABLED)),
      hard_override_enabled_(
          !IsMasked(display_options, HARD_OVERRIDE_DISABLED)),
      controller_(controller),
      user_made_decision_(false) {
  controller_->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller_->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  ssl_errors::RecordUMAStatistics(soft_override_enabled_, time_triggered_,
                                  request_url, cert_error_, *ssl_info_.cert);
}

SSLErrorUI::~SSLErrorUI() {
  // If the page is closing without an explicit decision, record it as not
  // proceeding.
  if (!user_made_decision_) {
    controller_->metrics_helper()->RecordUserDecision(
        MetricsHelper::DONT_PROCEED);
  }
  controller_->metrics_helper()->RecordShutdownMetrics();
}

void SSLErrorUI::PopulateStringsForHTML(base::Value::Dict& load_time_data) {
  // Shared with other errors.
  common_string_util::PopulateSSLLayoutStrings(cert_error_, load_time_data);
  common_string_util::PopulateSSLDebuggingStrings(ssl_info_, time_triggered_,
                                                  load_time_data);

  // Shared values for both the overridable and non-overridable versions.
  load_time_data.Set("bad_clock", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("tabTitle", l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data.Set("heading", l10n_util::GetStringUTF16(IDS_SSL_V2_HEADING));
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_SSL_V2_PRIMARY_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url_)));
  load_time_data.Set(
      "recurrentErrorParagraph",
      l10n_util::GetStringUTF16(IDS_SSL_V2_RECURRENT_ERROR_PARAGRAPH));
  load_time_data.Set("show_recurrent_error_paragraph",
                     controller_->HasSeenRecurrentError());

  if (soft_override_enabled_)
    PopulateOverridableStrings(load_time_data);
  else
    PopulateNonOverridableStrings(load_time_data);
}

const net::SSLInfo& SSLErrorUI::ssl_info() const {
  return ssl_info_;
}

const base::Time& SSLErrorUI::time_triggered() const {
  return time_triggered_;
}

ControllerClient* SSLErrorUI::controller() const {
  return controller_;
}

int SSLErrorUI::cert_error() const {
  return cert_error_;
}

void SSLErrorUI::PopulateOverridableStrings(base::Value::Dict& load_time_data) {
  DCHECK(soft_override_enabled_);

  std::u16string url(common_string_util::GetFormattedHostName(request_url_));
  ssl_errors::ErrorInfo error_info = ssl_errors::ErrorInfo::CreateError(
      ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_),
      ssl_info_.cert.get(), request_url_);

  load_time_data.Set("overridable", true);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("explanationParagraph", error_info.details());
  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));

// On iOS, offer to close the page instead of navigating to NTP when unable to
// go back. See crbug.com/1058476 for discussion.
#if BUILDFLAG(IS_IOS)
  if (!controller()->CanGoBack()) {
    load_time_data.Set(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_CLOSE_PAGE_BUTTON));
    load_time_data.Set("primary_button_close_page", true);
  }
#endif

  load_time_data.Set(
      "finalParagraph",
      l10n_util::GetStringFUTF16(IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, url));
}

void SSLErrorUI::PopulateNonOverridableStrings(
    base::Value::Dict& load_time_data) {
  DCHECK(!soft_override_enabled_);

  std::u16string url(common_string_util::GetFormattedHostName(request_url_));
  ssl_errors::ErrorInfo::ErrorType type =
      ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_);

  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(IDS_SSL_NONOVERRIDABLE_MORE, url));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(IDS_SSL_RELOAD));

  // Customize the help link depending on the specific error type.
  // Only mark as HSTS if none of the more specific error types apply,
  // and use INVALID as a fallback if no other string is appropriate.
  load_time_data.Set("errorType", type);
  int help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
  switch (type) {
    case ssl_errors::ErrorInfo::CERT_REVOKED:
      help_string = IDS_SSL_NONOVERRIDABLE_REVOKED;
      break;
    case ssl_errors::ErrorInfo::CERT_PINNED_KEY_MISSING:
      help_string = IDS_SSL_NONOVERRIDABLE_PINNED;
      break;
    case ssl_errors::ErrorInfo::CERT_INVALID:
      help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
      break;
    default:
      if (requested_strict_enforcement_)
        help_string = IDS_SSL_NONOVERRIDABLE_HSTS;
  }
  load_time_data.Set("finalParagraph",
                     l10n_util::GetStringFUTF16(help_string, url));
}

void SSLErrorUI::HandleCommand(SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_DONT_PROCEED: {
      controller_->metrics_helper()->RecordUserDecision(
          MetricsHelper::DONT_PROCEED);
      user_made_decision_ = true;
      controller_->GoBack();
      break;
    }
    case CMD_PROCEED: {
      if (hard_override_enabled_) {
        controller_->metrics_helper()->RecordUserDecision(
            MetricsHelper::PROCEED);
        controller_->Proceed();
        user_made_decision_ = true;
      }
      break;
    }
    case CMD_DO_REPORT: {
      controller_->SetReportingPreference(true);
      break;
    }
    case CMD_DONT_REPORT: {
      controller_->SetReportingPreference(false);
      break;
    }
    case CMD_SHOW_MORE_SECTION: {
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    }
    case CMD_OPEN_HELP_CENTER: {
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);

      // Add cert error code as a ref to support URL, this is used to expand the
      // right section if the user is redirected to chrome://connection-help.
      GURL::Replacements replacements;
      // This has to be stored in a separate variable, otherwise asan throws a
      // use-after-scope error
      std::string cert_error_string = base::NumberToString(cert_error_);
      replacements.SetRefStr(cert_error_string);
      // If |support_url_| is invalid, use the default help center url.
      controller_->OpenUrlInNewForegroundTab(
          (support_url_.is_valid()
               ? support_url_
               : controller_->GetBaseHelpCenterUrl().Resolve(kHelpPath))
              .ReplaceComponents(replacements));
      break;
    }
    case CMD_RELOAD: {
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::RELOAD);
      controller_->Reload();
      break;
    }
    case CMD_OPEN_REPORTING_PRIVACY: {
      controller_->OpenExtendedReportingPrivacyPolicy(true);
      break;
    }
    case CMD_OPEN_WHITEPAPER: {
      controller_->OpenExtendedReportingWhitepaper(true);
      break;
    }
    case CMD_OPEN_ENHANCED_PROTECTION_SETTINGS: {
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::OPEN_ENHANCED_PROTECTION);
      controller_->OpenEnhancedProtectionSettings();
      break;
    }
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_DIAGNOSTIC:
    case CMD_OPEN_LOGIN:
    case CMD_REPORT_PHISHING_ERROR:
    case CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
    case CMD_REQUEST_SITE_ACCESS_PERMISSION: {
      // Not supported by the SSL error page.
      DUMP_WILL_BE_NOTREACHED() << "Unsupported command: " << command;
      break;
    }
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND: {
      // Commands are for testing.
      break;
    }
  }
}

}  // security_interstitials
