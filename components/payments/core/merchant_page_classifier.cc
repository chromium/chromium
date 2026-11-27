// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/merchant_page_classifier.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/features.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace payments {

namespace {

std::unique_ptr<re2::RE2> CompileParamRegex(
    const base::FeatureParam<std::string>& param) {
  const std::string pattern = param.Get();
  if (pattern.empty()) {
    return nullptr;
  }
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  options.set_log_errors(false);
  options.set_never_capture(true);
  auto regex = std::make_unique<re2::RE2>(pattern, options);
  return regex->ok() ? std::move(regex) : nullptr;
}

bool Matches(const re2::RE2* regex, std::string_view text) {
  return regex && !text.empty() && re2::RE2::PartialMatch(text, *regex);
}

WebPaymentsClassifierResult ToWebPaymentsClassifierResult(bool url_matched,
                                                          bool title_matched) {
  if (url_matched && title_matched) {
    return WebPaymentsClassifierResult::kBothUrlAndTitle;
  }
  if (url_matched) {
    return WebPaymentsClassifierResult::kUrlOnly;
  }
  if (title_matched) {
    return WebPaymentsClassifierResult::kTitleOnly;
  }
  return WebPaymentsClassifierResult::kNoMatch;
}

}  // namespace

MerchantPageClassifier::MerchantPageClassifier()
    : order_confirmation_url_regex_(CompileParamRegex(
          features::kCheckoutTelemetryOrderConfirmationUrlRegex)),
      order_confirmation_title_regex_(CompileParamRegex(
          features::kCheckoutTelemetryOrderConfirmationTitleRegex)),
      checkout_url_regex_(
          CompileParamRegex(features::kCheckoutTelemetryCheckoutUrlRegex)),
      checkout_title_regex_(
          CompileParamRegex(features::kCheckoutTelemetryCheckoutTitleRegex)) {}

MerchantPageClassifier::~MerchantPageClassifier() = default;

MerchantClassificationResult MerchantPageClassifier::Classify(
    const GURL& url,
    std::u16string_view page_title) const {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return {};
  }

  const std::string title = base::UTF16ToUTF8(page_title);

  const WebPaymentsClassifierResult order_confirmation_source =
      ToWebPaymentsClassifierResult(
          Matches(order_confirmation_url_regex_.get(), url.spec()),
          Matches(order_confirmation_title_regex_.get(), title));
  if (order_confirmation_source != WebPaymentsClassifierResult::kNoMatch) {
    return {
        .page_type = MerchantPageType::kOrderConfirmationPage,
        .match_source = order_confirmation_source,
    };
  }

  const WebPaymentsClassifierResult checkout_source =
      ToWebPaymentsClassifierResult(
          Matches(checkout_url_regex_.get(), url.spec()),
          Matches(checkout_title_regex_.get(), title));
  if (checkout_source != WebPaymentsClassifierResult::kNoMatch) {
    return {
        .page_type = MerchantPageType::kCheckoutPage,
        .match_source = checkout_source,
    };
  }

  return {};
}

}  // namespace payments
