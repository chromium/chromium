// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_MOCK_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_MOCK_FEATURE_PROMO_CONTROLLER_H_

#include "base/feature_list.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/user_education/feature_promo_text_replacements.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockFeaturePromoController : public FeaturePromoController {
 public:
  MockFeaturePromoController();
  ~MockFeaturePromoController() override;

  // FeaturePromoController:
  MOCK_METHOD(bool,
              MaybeShowPromo,
              (const base::Feature&, BubbleCloseCallback),
              (override));
  MOCK_METHOD(bool,
              MaybeShowPromoWithTextReplacements,
              (const base::Feature&,
               FeaturePromoTextReplacements,
               BubbleCloseCallback),
              (override));
  MOCK_METHOD(bool, BubbleIsShowing, (const base::Feature&), (const, override));
  MOCK_METHOD(bool, CloseBubble, (const base::Feature&), (override));
  MOCK_METHOD(PromoHandle,
              CloseBubbleAndContinuePromo,
              (const base::Feature&),
              (override));
  MOCK_METHOD(void, FinishContinuedPromo, (), (override));
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_MOCK_FEATURE_PROMO_CONTROLLER_H_
