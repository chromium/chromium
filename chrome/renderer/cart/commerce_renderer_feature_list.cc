// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_renderer_feature_list.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "third_party/re2/src/re2/re2.h"

namespace commerce_renderer_feature {

namespace {

constexpr base::FeatureParam<std::string> kCouponPartnerMerchantPattern{
    &commerce_renderer_feature::kRetailCoupons,
    "coupon-partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

constexpr base::FeatureParam<std::string> kDiscountPartnerMerchantPattern{
    &ntp_features::kNtpChromeCartModule, "partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

const re2::RE2& GetCouponPartnerMerchantPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      kCouponPartnerMerchantPattern.Get(), options);
  return *instance;
}

bool IsCouponPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetCouponPartnerMerchantPattern());
}

const re2::RE2& GetDiscountPartnerMerchantPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      kDiscountPartnerMerchantPattern.Get(), options);
  return *instance;
}

bool IsDiscountPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetDiscountPartnerMerchantPattern());
}

}  // namespace

const base::Feature kRetailCoupons{"RetailCoupons",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

bool IsPartnerMerchant(const GURL& url) {
  return IsCouponPartnerMerchant(url) || IsDiscountPartnerMerchant(url);
}

}  // namespace commerce_renderer_feature
