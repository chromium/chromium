// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_CONTROLLER_H_

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

class MockFeaturePromoController : public FeaturePromoController {
 public:
  MockFeaturePromoController();
  ~MockFeaturePromoController() override;

  // FeaturePromoController:
  MOCK_METHOD(FeaturePromoResult,
              CanShowPromo,
              (const FeaturePromoParams&),
              (const, override));
  MOCK_METHOD(void, MaybeShowPromo, (FeaturePromoParams), (override));
  MOCK_METHOD(bool, MaybeShowStartupPromo, (FeaturePromoParams), (override));
  MOCK_METHOD(FeaturePromoResult,
              MaybeShowPromoForDemoPage,
              (FeaturePromoParams),
              (override));
  MOCK_METHOD(FeaturePromoStatus,
              GetPromoStatus,
              (const base::Feature&),
              (const, override));
  MOCK_METHOD(void,
              RecordPromoNotShown,
              (const char*, FeaturePromoResult::Failure),
              (const, override));
  MOCK_METHOD(const base::Feature*,
              GetCurrentPromoFeature,
              (),
              (const, override));
  MOCK_METHOD(const FeaturePromoSpecification*,
              GetCurrentPromoSpecificationForAnchor,
              (ui::ElementIdentifier),
              (const, override));
  MOCK_METHOD(bool,
              EndPromo,
              (const base::Feature&, EndFeaturePromoReason),
              (override));
  MOCK_METHOD(FeaturePromoHandle,
              CloseBubbleAndContinuePromo,
              (const base::Feature&),
              (override));
  MOCK_METHOD(void, FinishContinuedPromo, (const base::Feature&), (override));
  MOCK_METHOD(bool,
              HasPromoBeenDismissed,
              (const FeaturePromoParams&, FeaturePromoClosedReason*),
              (const, override));

  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockFeaturePromoController> weak_ptr_factory_{this};
};

class FeaturePromoParamsMatcher {
 public:
  explicit FeaturePromoParamsMatcher(const base::Feature& feature);
  FeaturePromoParamsMatcher(const FeaturePromoParamsMatcher&);
  ~FeaturePromoParamsMatcher();
  FeaturePromoParamsMatcher& operator=(const FeaturePromoParamsMatcher&);

  using is_gtest_matcher = void;

  bool MatchAndExplain(const FeaturePromoParams&, std::ostream*) const;
  void DescribeTo(std::ostream*) const;
  void DescribeNegationTo(std::ostream*) const;

 private:
  base::raw_ref<const base::Feature> feature_;
};

template <typename... Args>
testing::Matcher<FeaturePromoParams> MatchFeaturePromoParams(Args&&... args) {
  return testing::Matcher<FeaturePromoParams>(
      FeaturePromoParamsMatcher(std::forward<Args>(args)...));
}

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_CONTROLLER_H_
