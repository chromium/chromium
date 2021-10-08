// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CART_COMMERCE_RENDERER_FEATURE_LIST_H_
#define CHROME_RENDERER_CART_COMMERCE_RENDERER_FEATURE_LIST_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/search/ntp_features.h"
#include "url/gurl.h"

namespace commerce_renderer_feature {
extern const base::Feature kRetailCoupons;

// Whether to use OptimizationGuide to optimize renderer signal collection.
constexpr base::FeatureParam<bool> kOptimizeRendererSignal(
    &ntp_features::kNtpChromeCartModule,
    "optimize-renderer-signal",
    false);

// Check if a URL belongs to a partner merchant of any type of discount.
bool IsPartnerMerchant(const GURL& url);
}  // namespace commerce_renderer_feature

#endif  // CHROME_RENDERER_CART_COMMERCE_RENDERER_FEATURE_LIST_H_
