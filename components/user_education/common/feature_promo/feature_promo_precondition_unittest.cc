// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"

#include "base/functional/callback_forward.h"
#include "base/test/gtest_util.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/typed_identifier.h"

namespace user_education {

namespace {
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kTestId);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kTestId2);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kTestId3);
constexpr FeaturePromoResult::Failure kPrecondFailure =
    FeaturePromoResult::Failure::kError;
constexpr FeaturePromoResult::Failure kPrecondFailure2 =
    FeaturePromoResult::Failure::kBlockedByContext;
constexpr FeaturePromoResult::Failure kPrecondFailure3 =
    FeaturePromoResult::Failure::kBlockedByUi;
constexpr char kPrecondName[] = "Precond";
constexpr char kPrecondName2[] = "Precond2";
constexpr char kPrecondName3[] = "Precond3";

DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(int, kIntegerData);
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(std::string, kStringData);
}  // namespace

TEST(FeaturePromoPreconditionTest, SetAndGetCachedData) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondFailure,
                                           kPrecondName, false);
  precond1.InitCache(kIntegerData);
  precond1.InitCache(kStringData);
  EXPECT_EQ(0, precond1.GetCachedData(kIntegerData));
  EXPECT_EQ("", precond1.GetCachedData(kStringData));
  precond1.GetCachedData(kIntegerData) = 2;
  precond1.GetCachedData(kStringData) = "3";
  EXPECT_EQ(2, precond1.GetCachedData(kIntegerData));
  EXPECT_EQ("3", precond1.GetCachedData(kStringData));
}

TEST(FeaturePromoPreconditionTest, GetCachedDataCrashesIfDataNotPresent) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondFailure,
                                           kPrecondName, false);
  EXPECT_CHECK_DEATH(precond1.GetCachedData(kIntegerData));
}

TEST(FeaturePromoPreconditionTest, ExtractCachedData) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondFailure,
                                           kPrecondName, false);
  precond1.InitCache(kIntegerData, kStringData);
  precond1.GetCachedData(kIntegerData) = 2;
  precond1.GetCachedData(kStringData) = "3";

  internal::PreconditionData::Collection coll;
  precond1.ExtractCachedData(coll);
  EXPECT_EQ(2, *internal::PreconditionData::Get(coll, kIntegerData));
  EXPECT_EQ("3", *internal::PreconditionData::Get(coll, kStringData));
}

TEST(FeaturePromoPreconditionTest, GetAfterExtractCachedDataFails) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondFailure,
                                           kPrecondName, false);
  precond1.InitCache(kIntegerData, kStringData);
  precond1.GetCachedData(kIntegerData) = 2;
  precond1.GetCachedData(kStringData) = "3";

  internal::PreconditionData::Collection coll;
  precond1.ExtractCachedData(coll);
  EXPECT_CHECK_DEATH(precond1.GetCachedData(kIntegerData));
}

TEST(FeaturePromoPreconditionTest, CachingFeaturePromoPrecondition) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondFailure,
                                           kPrecondName, false);
  EXPECT_EQ(false, precond1.IsAllowed());
  EXPECT_EQ(kTestId, precond1.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond1.GetFailure());
  EXPECT_EQ(kPrecondName, precond1.GetDescription());

  precond1.set_is_allowed_for_testing(true);
  EXPECT_EQ(true, precond1.IsAllowed());
  precond1.set_is_allowed_for_testing(false);
  EXPECT_EQ(false, precond1.IsAllowed());

  CachingFeaturePromoPrecondition precond2(kTestId, kPrecondFailure,
                                           kPrecondName, true);
  EXPECT_EQ(true, precond2.IsAllowed());
}

TEST(FeaturePromoPreconditionTest, CallbackFeaturePromoPrecondition) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<bool()>, callback);
  CallbackFeaturePromoPrecondition precond(kTestId, kPrecondFailure,
                                           kPrecondName, callback.Get());
  EXPECT_EQ(kTestId, precond.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond.GetFailure());
  EXPECT_EQ(kPrecondName, precond.GetDescription());

  EXPECT_CALL(callback, Run).WillOnce(testing::Return(true));
  EXPECT_EQ(true, precond.IsAllowed());

  EXPECT_CALL(callback, Run).WillOnce(testing::Return(false));
  EXPECT_EQ(false, precond.IsAllowed());
}

TEST(FeaturePromoPreconditionTest, ForwardingFeaturePromoPrecondition) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondFailure,
                                           kPrecondName, false);
  ForwardingFeaturePromoPrecondition precond2(precond1);
  EXPECT_EQ(false, precond2.IsAllowed());
  EXPECT_EQ(kTestId, precond2.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond2.GetFailure());
  EXPECT_EQ(kPrecondName, precond2.GetDescription());

  precond1.set_is_allowed_for_testing(true);
  EXPECT_EQ(true, precond2.IsAllowed());
  precond1.set_is_allowed_for_testing(false);
  EXPECT_EQ(false, precond2.IsAllowed());
}

namespace {

std::unique_ptr<FeaturePromoPrecondition> MakeTestPrecondition(
    CachingFeaturePromoPrecondition*& ptr_out,
    FeaturePromoPrecondition::Identifier identifier,
    FeaturePromoResult::Failure failure,
    std::string description) {
  CHECK_EQ(nullptr, ptr_out);
  auto result = std::make_unique<CachingFeaturePromoPrecondition>(
      identifier, failure, std::move(description), true);
  ptr_out = result.get();
  return result;
}

}  // namespace

TEST(FeaturePromoPreconditionTest, FeaturePromoPreconditionList) {
  CachingFeaturePromoPrecondition* precond1 = nullptr;
  CachingFeaturePromoPrecondition* precond2 = nullptr;
  CachingFeaturePromoPrecondition* precond3 = nullptr;
  using CheckResult = FeaturePromoPreconditionList::CheckResult;
  FeaturePromoPreconditionList list(
      MakeTestPrecondition(precond1, kTestId, kPrecondFailure, kPrecondName),
      MakeTestPrecondition(precond2, kTestId2, kPrecondFailure2, kPrecondName2),
      MakeTestPrecondition(precond3, kTestId3, kPrecondFailure3,
                           kPrecondName3));

  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());

  precond3->set_is_allowed_for_testing(false);
  // true, true, false
  EXPECT_EQ(CheckResult(precond3->GetFailure(), precond3->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_is_allowed_for_testing(false);
  // false, true, false
  EXPECT_EQ(CheckResult(precond1->GetFailure(), precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_is_allowed_for_testing(false);
  // false, false, false
  EXPECT_EQ(CheckResult(precond1->GetFailure(), precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_is_allowed_for_testing(true);
  // true, false, false
  EXPECT_EQ(CheckResult(precond2->GetFailure(), precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond3->set_is_allowed_for_testing(true);
  // true, false, true
  EXPECT_EQ(CheckResult(precond2->GetFailure(), precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_is_allowed_for_testing(true);
  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());
}

// Same as previous test, but add the preconditions individually.
TEST(FeaturePromoPreconditionTest,
     FeaturePromoPreconditionList_AddPrecondition) {
  CachingFeaturePromoPrecondition* precond1 = nullptr;
  CachingFeaturePromoPrecondition* precond2 = nullptr;
  CachingFeaturePromoPrecondition* precond3 = nullptr;
  using CheckResult = FeaturePromoPreconditionList::CheckResult;
  FeaturePromoPreconditionList list(
      MakeTestPrecondition(precond1, kTestId, kPrecondFailure, kPrecondName));
  list.AddPrecondition(MakeTestPrecondition(precond2, kTestId2,
                                            kPrecondFailure2, kPrecondName2));
  list.AddPrecondition(MakeTestPrecondition(precond3, kTestId3,
                                            kPrecondFailure3, kPrecondName3));

  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());

  precond3->set_is_allowed_for_testing(false);
  // true, true, false
  EXPECT_EQ(CheckResult(precond3->GetFailure(), precond3->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_is_allowed_for_testing(false);
  // false, true, false
  EXPECT_EQ(CheckResult(precond1->GetFailure(), precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_is_allowed_for_testing(false);
  // false, false, false
  EXPECT_EQ(CheckResult(precond1->GetFailure(), precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_is_allowed_for_testing(true);
  // true, false, false
  EXPECT_EQ(CheckResult(precond2->GetFailure(), precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond3->set_is_allowed_for_testing(true);
  // true, false, true
  EXPECT_EQ(CheckResult(precond2->GetFailure(), precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_is_allowed_for_testing(true);
  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());
}

// Same as previous test, but add the preconditions from another list.
TEST(FeaturePromoPreconditionTest, FeaturePromoPreconditionList_AppendAll) {
  CachingFeaturePromoPrecondition* precond1 = nullptr;
  CachingFeaturePromoPrecondition* precond2 = nullptr;
  CachingFeaturePromoPrecondition* precond3 = nullptr;
  using CheckResult = FeaturePromoPreconditionList::CheckResult;
  FeaturePromoPreconditionList temp(
      MakeTestPrecondition(precond2, kTestId2, kPrecondFailure2, kPrecondName2),
      MakeTestPrecondition(precond3, kTestId3, kPrecondFailure3,
                           kPrecondName3));
  FeaturePromoPreconditionList list(
      MakeTestPrecondition(precond1, kTestId, kPrecondFailure, kPrecondName));
  list.AppendAll(std::move(temp));

  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());

  precond3->set_is_allowed_for_testing(false);
  // true, true, false
  EXPECT_EQ(CheckResult(precond3->GetFailure(), precond3->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_is_allowed_for_testing(false);
  // false, true, false
  EXPECT_EQ(CheckResult(precond1->GetFailure(), precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_is_allowed_for_testing(false);
  // false, false, false
  EXPECT_EQ(CheckResult(precond1->GetFailure(), precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_is_allowed_for_testing(true);
  // true, false, false
  EXPECT_EQ(CheckResult(precond2->GetFailure(), precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond3->set_is_allowed_for_testing(true);
  // true, false, true
  EXPECT_EQ(CheckResult(precond2->GetFailure(), precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_is_allowed_for_testing(true);
  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());
}

TEST(FeaturePromoPreconditionTest,
     FeaturePromoPreconditionList_ExtractCachedData) {
  auto precond1 = std::make_unique<CachingFeaturePromoPrecondition>(
      kTestId, kPrecondFailure, kPrecondName, true);
  auto precond2 = std::make_unique<CachingFeaturePromoPrecondition>(
      kTestId2, kPrecondFailure2, kPrecondName2, true);
  precond1->InitCache(kIntegerData);
  precond2->InitCache(kStringData);
  precond1->GetCachedData(kIntegerData) = 2;
  precond2->GetCachedData(kStringData) = "3";

  FeaturePromoPreconditionList list(std::move(precond1), std::move(precond2));
  internal::PreconditionData::Collection coll;
  list.ExtractCachedData(coll);

  EXPECT_EQ(2, *internal::PreconditionData::Get(coll, kIntegerData));
  EXPECT_EQ("3", *internal::PreconditionData::Get(coll, kStringData));
}

}  // namespace user_education
