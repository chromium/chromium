// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/common_string_util.h"

#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/net_errors.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

namespace common_string_util {

std::u16string GetFormattedHostName(const GURL& gurl) {
  std::u16string host = url_formatter::IDNToUnicode(gurl.host());
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&host);
  return host;
}

void PopulateSSLLayoutStrings(int cert_error, base::Value* load_time_data) {
  load_time_data->SetStringKey("type", "SSL");
  load_time_data->SetStringKey("errorCode", net::ErrorToString(cert_error));
  load_time_data->SetStringKey(
      "openDetails", l10n_util::GetStringUTF16(IDS_SSL_OPEN_DETAILS_BUTTON));
  load_time_data->SetStringKey(
      "closeDetails", l10n_util::GetStringUTF16(IDS_SSL_CLOSE_DETAILS_BUTTON));
  // Not used by most interstitials; can be overridden by individual
  // interstitials as needed.
  load_time_data->SetStringKey("recurrentErrorParagraph", "");
  load_time_data->SetBoolKey("show_recurrent_error_paragraph", false);
  load_time_data->SetStringKey(
      "optInLink",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data->SetStringKey(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
}

void PopulateSSLDebuggingStrings(const net::SSLInfo ssl_info,
                                 const base::Time time_triggered,
                                 base::Value* load_time_data) {
  load_time_data->SetStringKey("subject",
                               ssl_info.cert->subject().GetDisplayName());
  load_time_data->SetStringKey("issuer",
                               ssl_info.cert->issuer().GetDisplayName());
  load_time_data->SetStringKey(
      "expirationDate",
      base::TimeFormatShortDate(ssl_info.cert->valid_expiry()));
  load_time_data->SetStringKey("currentDate",
                               base::TimeFormatShortDate(time_triggered));
  std::vector<std::string> sct_list;
  for (const auto& sct_status : ssl_info.signed_certificate_timestamps) {
    std::string sct_info = "\n\nSCT " + sct_status.sct->log_description + " (" +
                           net::ct::OriginToString(sct_status.sct->origin) +
                           ", " + net::ct::StatusToString(sct_status.status) +
                           ")";
    sct_list.push_back(sct_info);
  }
  load_time_data->SetStringKey("ct", base::StrCat(sct_list));
  std::vector<std::string> encoded_chain;
  ssl_info.cert->GetPEMEncodedChain(&encoded_chain);
  load_time_data->SetStringKey("pem", base::StrCat(encoded_chain));
}

void PopulateLegacyTLSStrings(base::Value* load_time_data,
                              const std::u16string& hostname) {
  load_time_data->SetStringKey("tabTitle",
                               l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data->SetStringKey(
      "heading", l10n_util::GetStringUTF16(IDS_LEGACY_TLS_HEADING));
  load_time_data->SetStringKey(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data->SetStringKey(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_LEGACY_TLS_PRIMARY_PARAGRAPH));
  load_time_data->SetStringKey(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_LEGACY_TLS_EXPLANATION));
  load_time_data->SetStringKey(
      "finalParagraph", l10n_util::GetStringFUTF16(
                            IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, hostname));
}

}  // namespace common_string_util

}  // namespace security_interstitials
