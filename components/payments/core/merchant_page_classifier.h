// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_MERCHANT_PAGE_CLASSIFIER_H_
#define COMPONENTS_PAYMENTS_CORE_MERCHANT_PAGE_CLASSIFIER_H_

#include <memory>
#include <string_view>

class GURL;

namespace re2 {
class RE2;
}  // namespace re2

namespace payments {

// Page types detected by MerchantPageClassifier.
enum class MerchantPageType {
  kNone = 0,
  kCheckoutPage = 1,
  kOrderConfirmationPage = 2,
};

// Which inputs matched in MerchantPageClassifier, recorded to UMA and UKM.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebPaymentsClassifierResult {
  kNoMatch = 0,
  kUrlOnly = 1,
  kTitleOnly = 2,
  kBothUrlAndTitle = 3,
  kMaxValue = kBothUrlAndTitle,
};

// Result returned by MerchantPageClassifier::Classify().
struct MerchantClassificationResult {
  friend bool operator==(const MerchantClassificationResult&,
                         const MerchantClassificationResult&) = default;

  MerchantPageType page_type = MerchantPageType::kNone;
  WebPaymentsClassifierResult match_source =
      WebPaymentsClassifierResult::kNoMatch;
};

// Classifies a merchant webpage as a checkout page, an order confirmation
// page, or neither based on its URL and page title.
class MerchantPageClassifier {
 public:
  MerchantPageClassifier();
  MerchantPageClassifier(const MerchantPageClassifier&) = delete;
  MerchantPageClassifier& operator=(const MerchantPageClassifier&) = delete;
  ~MerchantPageClassifier();

  // Only HTTP and HTTPS pages are classified. The full URL (including query
  // and fragment) and the page title are matched case-insensitively. Order
  // confirmation rules run before checkout rules so URLs like
  // `/checkout/thank-you` return `kOrderConfirmationPage`.
  MerchantClassificationResult Classify(const GURL& url,
                                        std::u16string_view page_title) const;

 private:
  std::unique_ptr<re2::RE2> order_confirmation_url_regex_;
  std::unique_ptr<re2::RE2> order_confirmation_title_regex_;
  std::unique_ptr<re2::RE2> checkout_url_regex_;
  std::unique_ptr<re2::RE2> checkout_title_regex_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_MERCHANT_PAGE_CLASSIFIER_H_
