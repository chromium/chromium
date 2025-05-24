// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_service_url.h"

#include <string>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

// Service URLs used for calls to Google Payments endpoints.
const char kProdPaymentsServiceUrl[] = "https://payments.google.com/";
const char kSandboxPaymentsSecureServiceUrl[] =
    "https://payments.sandbox.google.com/";

// Origins of execution used by Google Pay's pay.js script
const char kProdGooglePayScriptOrigin[] = "https://pay.google.com/";
const char kSandboxGooglePayScriptOrigin[] = "https://pay.sandbox.google.com/";

// URLs used when opening the Payment methods management page from
// chrome://settings/payments.
const char kProdPaymentsManageCardsUrl[] =
    "https://pay.google.com/"
    "pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
    "payment_methods";
const char kSandboxPaymentsManageCardsUrl[] =
    "https://pay.sandbox.google.com/"
    "pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
    "payment_methods";
// LINT.IfChange
const char kVirtualCardEnrollmentSupportUrl[] =
    "https://support.google.com/googlepay/answer/11234179";
// LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/ChromeStringConstants.java)

// BNPL provider terms support URLs.
// TODO(crbug.com/397446359): Change URL once terms redirect support pages are
// finalized.
constexpr char kBnplAffirmTermsUrl[] =
    "https://support.google.com/googlepay?p=bnpl_autofill_chrome";
constexpr char kBnplZipTermsUrl[] =
    "https://support.google.com/googlepay?p=bnpl_autofill_chrome";
}  // namespace

namespace payments {

using IssuerId = autofill::BnplIssuer::IssuerId;

bool IsPaymentsProductionEnabled() {
  // If the command line flag exists, it takes precedence.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string sandbox_enabled(
      command_line->GetSwitchValueASCII(switches::kWalletServiceUseSandbox));
  return sandbox_enabled.empty() || sandbox_enabled != "1";
}

GURL GetBaseSecureUrl() {
  return GURL(IsPaymentsProductionEnabled() ? kProdPaymentsServiceUrl
                                            : kSandboxPaymentsSecureServiceUrl);
}

url::Origin GetGooglePayScriptOrigin() {
  return url::Origin::Create(GURL(IsPaymentsProductionEnabled()
                                      ? kProdGooglePayScriptOrigin
                                      : kSandboxGooglePayScriptOrigin));
}

GURL GetManageInstrumentsUrl() {
  return GURL(IsPaymentsProductionEnabled() ? kProdPaymentsManageCardsUrl
                                            : kSandboxPaymentsManageCardsUrl);
}

GURL GetManageInstrumentUrl(int64_t instrument_id) {
  GURL url = GetManageInstrumentsUrl();
  std::string new_query =
      base::StrCat({url.query(), "&id=", base::NumberToString(instrument_id)});
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query);
  return url.ReplaceComponents(replacements);
}

GURL GetManageAddressesUrl() {
  // Billing addresses are now managed as a part of the payment instrument.
  return GetManageInstrumentsUrl();
}

GURL GetVirtualCardEnrollmentSupportUrl() {
  return GURL(kVirtualCardEnrollmentSupportUrl);
}

GURL GetBnplTermsUrl(IssuerId issuer_id) {
  switch (issuer_id) {
    case IssuerId::kBnplAffirm:
      return GURL(kBnplAffirmTermsUrl);
    case IssuerId::kBnplZip:
      return GURL(kBnplZipTermsUrl);
    // TODO(crbug.com/408268581): Handle Afterpay issuer enum value when adding
    // Afterpay to the BNPL flow.
    case IssuerId::kBnplAfterpay:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace payments
}  // namespace autofill
