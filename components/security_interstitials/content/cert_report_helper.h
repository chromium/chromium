// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CERT_REPORT_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CERT_REPORT_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/security_interstitials/content/certificate_error_report.h"
#include "components/security_interstitials/core/controller_client.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace security_interstitials {
class MetricsHelper;
}

class SSLCertReporter;

// CertReportHelper helps SSL interstitials report invalid certificate
// chains. Its main methods are:
// - HandleReportingCommands() which processes commands from the interstitial
// page that are related to certificate reporting: toggling the preference, and
// proceeding or going back.
// - FinishCertCollection() which should be called when an interstitial is
// closing to send a certificate report.
class CertReportHelper {
 public:
  // Constants for the HTTPSErrorReporter Finch experiment
  static const char kFinchExperimentName[];
  static const char kFinchGroupShowPossiblySend[];
  static const char kFinchGroupDontShowDontSend[];
  static const char kFinchParamName[];

  CertReportHelper(
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      content::WebContents* web_contents,
      const GURL& request_url,
      const net::SSLInfo& ssl_info,
      CertificateErrorReport::InterstitialReason interstitial_reason,
      bool overridable,
      const base::Time& interstitial_time,
      bool can_show_enhanced_protection_message,
      security_interstitials::MetricsHelper* metrics_helper);

  CertReportHelper(const CertReportHelper&) = delete;
  CertReportHelper& operator=(const CertReportHelper&) = delete;

  virtual ~CertReportHelper();

  // This method can be called by tests to fake an official build (reports are
  // only sent from official builds).
  static void SetFakeOfficialBuildForTesting();

  // Populates data that JavaScript code on the interstitial uses to show
  // the checkbox.
  void PopulateExtendedReportingOption(base::Value::Dict& load_time_data);

  // Populates data that JavaScript code on the interstitial uses to show
  // the enhanced protection message.
  void PopulateEnhancedProtectionMessage(base::Value::Dict& load_time_data);

  // Allows tests to inject a mock reporter.
  void SetSSLCertReporterForTesting(
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

  // Handles reporting-related commands from the interstitial page: toggling the
  // report preference, and sending reports on proceed/do not proceed. For
  // preference-toggling commands, this method updates the corresponding prefs
  // in |pref_service|. For proceeding/going back, the user action is saved to
  // be reported when FinishCertCollection() is called.
  void HandleReportingCommands(
      security_interstitials::SecurityInterstitialCommand command,
      PrefService* pref_service);

  // Sends a report about an invalid certificate to the server.
  void FinishCertCollection();

  using ClientDetailsCallback =
      base::RepeatingCallback<void(CertificateErrorReport* report)>;

  void set_client_details_callback(ClientDetailsCallback callback) {
    client_details_callback_ = callback;
  }

 private:
  // Checks whether a checkbox should be shown on the page that allows
  // the user to opt in to Safe Browsing extended reporting.
  bool ShouldShowCertificateReporterCheckbox();

  // Checks whether a enhanced protection message should be shown on the page.
  bool ShouldShowEnhancedProtectionMessage();

  // Returns true if a certificate report should be sent for the SSL
  // error for this page.
  bool ShouldReportCertificateError();

  // Handles reports of invalid SSL certificates.
  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;
  // The WebContents for which this helper sends reports.
  raw_ptr<content::WebContents> web_contents_;
  // The URL for which this helper sends reports.
  const GURL request_url_;
  // The SSLInfo used in this helper's report.
  const net::SSLInfo ssl_info_;
  // The reason for the interstitial, included in this helper's report.
  CertificateErrorReport::InterstitialReason interstitial_reason_;
  // True if the user was given the option to proceed through the
  // certificate chain error being reported.
  bool overridable_;
  // The time at which the interstitial was constructed.
  const base::Time interstitial_time_;
  // Whether to show enhanced protection message in the interstitial. If it is
  // set to false, the message will be hidden. If it is set to true, other
  // states and user preferences will still be checked.
  // TODO(crbug.com/1078381): This is currently set to false in WebLayer and
  // true in other platforms. Remove this member once WebLayer supports enhanced
  // protection.
  bool can_show_enhanced_protection_message_;
  // Helpful for recording metrics about cert reports.
  raw_ptr<security_interstitials::MetricsHelper> metrics_helper_;
  // Appends additional details to a report.
  ClientDetailsCallback client_details_callback_;
  // Default to DID_NOT_PROCEED. If no user action is processed via
  // HandleReportingCommands() before FinishCertCollection(), then act as if the
  // user did not proceed for reporting purposes -- e.g. closing the tab without
  // taking an action on the interstitial is counted as not proceeding.
  CertificateErrorReport::ProceedDecision user_action_ =
      CertificateErrorReport::USER_DID_NOT_PROCEED;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CERT_REPORT_HELPER_H_
