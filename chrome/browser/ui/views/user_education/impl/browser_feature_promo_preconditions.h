// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kWindowActivePrecondition);

// Requires that the window a promo will be shown in is active.
class WindowActivePrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  WindowActivePrecondition();
  ~WindowActivePrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ComputedData& data) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_
