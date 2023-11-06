// Copyright 2015 The Chromium Authors
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

namespace security_interstitials::common_string_util {

std::u16string GetFormattedHostName(const GURL& gurl) {
  std::u16string host = url_formatter::IDNToUnicode(gurl.host());
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&host);
  return host;
}

void PopulateSSLLayoutStrings(int cert_error,
                              base::Value::Dict& load_time_data) {
  load_time_data.Set("type", "SSL");
  load_time_data.Set("errorCode", net::ErrorToString(cert_error));
  load_time_data.Set("openDetails",
                     l10n_util::GetStringUTF16(IDS_SSL_OPEN_DETAILS_BUTTON));
  load_time_data.Set("closeDetails",
                     l10n_util::GetStringUTF16(IDS_SSL_CLOSE_DETAILS_BUTTON));
  // Not used by most interstitials; can be overridden by individual
  // interstitials as needed.
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("show_recurrent_error_paragraph", false);
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
}

void PopulateSSLDebuggingStrings(const net::SSLInfo ssl_info,
                                 const base::Time time_triggered,
                                 base::Value::Dict& load_time_data) {
  load_time_data.Set("subject", ssl_info.cert->subject().GetDisplayName());
  load_time_data.Set("issuer", ssl_info.cert->issuer().GetDisplayName());
  load_time_data.Set("expirationDate",
                     base::TimeFormatShortDate(ssl_info.cert->valid_expiry()));
  load_time_data.Set("currentDate", base::TimeFormatShortDate(time_triggered));
  std::string sct_list;
  for (const auto& sct_status : ssl_info.signed_certificate_timestamps) {
    base::StrAppend(&sct_list,
                    {"\n\nSCT ", sct_status.sct->log_description, " (",
                     net::ct::OriginToString(sct_status.sct->origin), ", ",
                     net::ct::StatusToString(sct_status.status), ")"});
  }
  load_time_data.Set("ct", std::move(sct_list));
  std::vector<std::string> encoded_chain;
  ssl_info.cert->GetPEMEncodedChain(&encoded_chain);
  load_time_data.Set("pem", base::StrCat(encoded_chain));
}

}  // namespace security_interstitials::common_string_util
