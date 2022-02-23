// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_MOCK_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_MOCK_FEATURE_PROMO_CONTROLLER_H_

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockFeaturePromoController : public FeaturePromoController {
 public:
  MockFeaturePromoController();
  ~MockFeaturePromoController() override;

  // FeaturePromoController:
  MOCK_METHOD(bool,
              MaybeShowPromo,
              (const base::Feature&,
               FeaturePromoSpecification::StringReplacements,
               BubbleCloseCallback),
              (override));
  MOCK_METHOD(bool,
              MaybeShowPromoForDemoPage,
              (const base::Feature*,
               FeaturePromoSpecification::StringReplacements,
               BubbleCloseCallback),
              (override));
  MOCK_METHOD(bool,
              IsPromoActive,
              (const base::Feature&, bool),
              (const, override));
  MOCK_METHOD(bool, CloseBubble, (const base::Feature&), (override));
  MOCK_METHOD(PromoHandle,
              CloseBubbleAndContinuePromo,
              (const base::Feature&),
              (override));
  MOCK_METHOD(void,
              FinishContinuedPromo,
              (const base::Feature* iph_feature),
              (override));

  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockFeaturePromoController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_MOCK_FEATURE_PROMO_CONTROLLER_H_
