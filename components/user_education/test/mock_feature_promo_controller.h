// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_CONTROLLER_H_

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

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
              MaybeShowStartupPromo,
              (const base::Feature&,
               FeaturePromoSpecification::StringReplacements,
               StartupPromoCallback,
               BubbleCloseCallback),
              (override));
  MOCK_METHOD(bool,
              MaybeShowPromoForDemoPage,
              (const base::Feature*,
               FeaturePromoSpecification::StringReplacements,
               BubbleCloseCallback),
              (override));
  MOCK_METHOD(FeaturePromoStatus,
              GetPromoStatus,
              (const base::Feature&),
              (const, override));
  MOCK_METHOD(bool, EndPromo, (const base::Feature&), (override));
  MOCK_METHOD(FeaturePromoHandle,
              CloseBubbleAndContinuePromo,
              (const base::Feature&),
              (override));
  MOCK_METHOD(void,
              FinishContinuedPromo,
              (const base::Feature& iph_feature),
              (override));

  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockFeaturePromoController> weak_ptr_factory_{this};
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_CONTROLLER_H_
