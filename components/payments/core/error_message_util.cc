// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_message_util.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace payments {

namespace {

template <class Collection>
std::string concatNamesWithQuotesAndCommma(const Collection& names) {
  std::vector<std::string> with_quotes(names.size());
  std::ranges::transform(
      names, with_quotes.begin(),
      [](const std::string& method_name) { return "\"" + method_name + "\""; });
  std::string result = base::JoinString(with_quotes, ", ");
  return result;
}

}  // namespace

std::string GetNotSupportedErrorMessage(const std::set<std::string>& methods) {
  if (methods.empty())
    return errors::kGenericPaymentMethodNotSupportedMessage;

  std::string output;
  bool replaced = base::ReplaceChars(
      methods.size() == 1 ? errors::kSinglePaymentMethodNotSupportedFormat
                          : errors::kMultiplePaymentMethodsNotSupportedFormat,
      "$", concatNamesWithQuotesAndCommma(methods), &output);
  DCHECK(replaced);
  return output;
}

std::string GetAppsSkippedForPartialDelegationErrorMessage(
    const std::vector<std::string>& skipped_app_names) {
  std::string output;
  bool replaced = base::ReplaceChars(
      errors::kSkipAppForPartialDelegation, "$",
      concatNamesWithQuotesAndCommma(skipped_app_names), &output);

  DCHECK(replaced);
  return output;
}

std::string GenerateHttpStatusCodeError(const GURL& url,
                                        int http_response_code) {
  std::string_view status_string =
      net::GetHttpReasonPhrase(http_response_code, /*default_value=*/"Unknown");
  return base::ReplaceStringPlaceholders(
      errors::kPaymentManifestDownloadFailedWithHttpStatusCode,
      {url.spec(), base::NumberToString(http_response_code),
       std::string(status_string)},
      nullptr);
}

}  // namespace payments
