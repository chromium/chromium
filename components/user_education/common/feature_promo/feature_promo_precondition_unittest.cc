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
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
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
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  EXPECT_CHECK_DEATH(precond1.GetCachedData(kIntegerData));
}

TEST(FeaturePromoPreconditionTest, ExtractCachedData) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  precond1.InitCache(kIntegerData, kStringData);
  precond1.GetCachedData(kIntegerData) = 2;
  precond1.GetCachedData(kStringData) = "3";

  internal::PreconditionData::Collection coll;
  precond1.ExtractCachedData(coll);
  EXPECT_EQ(2, *internal::PreconditionData::Get(coll, kIntegerData));
  EXPECT_EQ("3", *internal::PreconditionData::Get(coll, kStringData));
}

TEST(FeaturePromoPreconditionTest, GetAfterExtractCachedDataFails) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  precond1.InitCache(kIntegerData, kStringData);
  precond1.GetCachedData(kIntegerData) = 2;
  precond1.GetCachedData(kStringData) = "3";

  internal::PreconditionData::Collection coll;
  precond1.ExtractCachedData(coll);
  EXPECT_CHECK_DEATH(precond1.GetCachedData(kIntegerData));
}

TEST(FeaturePromoPreconditionTest, CachingFeaturePromoPrecondition) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  EXPECT_EQ(kTestId, precond1.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond1.CheckPrecondition());
  EXPECT_EQ(kPrecondName, precond1.GetDescription());

  precond1.set_check_result_for_testing(FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(), precond1.CheckPrecondition());
  precond1.set_check_result_for_testing(kPrecondFailure);
  EXPECT_EQ(kPrecondFailure, precond1.CheckPrecondition());

  CachingFeaturePromoPrecondition precond2(kTestId, kPrecondName,
                                           FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(), precond2.CheckPrecondition());
}

TEST(FeaturePromoPreconditionTest, CallbackFeaturePromoPrecondition) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<FeaturePromoResult()>,
                         callback);
  CallbackFeaturePromoPrecondition precond(kTestId, kPrecondName,
                                           callback.Get());
  EXPECT_EQ(kTestId, precond.GetIdentifier());
  EXPECT_EQ(kPrecondName, precond.GetDescription());

  EXPECT_CALL(callback, Run)
      .WillOnce(testing::Return(FeaturePromoResult::Success()));
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());

  EXPECT_CALL(callback, Run).WillOnce(testing::Return(kPrecondFailure));
  EXPECT_EQ(kPrecondFailure, precond.CheckPrecondition());
}

TEST(FeaturePromoPreconditionTest, ForwardingFeaturePromoPrecondition) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  ForwardingFeaturePromoPrecondition precond2(precond1);
  EXPECT_EQ(kTestId, precond2.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond2.CheckPrecondition());
  EXPECT_EQ(kPrecondName, precond2.GetDescription());

  precond1.set_check_result_for_testing(FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(), precond2.CheckPrecondition());
  precond1.set_check_result_for_testing(kPrecondFailure);
  EXPECT_EQ(kPrecondFailure, precond2.CheckPrecondition());
}

namespace {

std::unique_ptr<FeaturePromoPrecondition> MakeTestPrecondition(
    CachingFeaturePromoPrecondition*& ptr_out,
    FeaturePromoPrecondition::Identifier identifier,
    std::string description) {
  CHECK_EQ(nullptr, ptr_out);
  auto result = std::make_unique<CachingFeaturePromoPrecondition>(
      identifier, std::move(description), FeaturePromoResult::Success());
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
      MakeTestPrecondition(precond1, kTestId, kPrecondName),
      MakeTestPrecondition(precond2, kTestId2, kPrecondName2),
      MakeTestPrecondition(precond3, kTestId3, kPrecondName3));

  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());

  precond3->set_check_result_for_testing(kPrecondFailure3);
  // true, true, false
  EXPECT_EQ(CheckResult(kPrecondFailure3, precond3->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_check_result_for_testing(kPrecondFailure);
  // false, true, false
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_check_result_for_testing(kPrecondFailure2);
  // false, false, false
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, false
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond3->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, true
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_check_result_for_testing(FeaturePromoResult::Success());
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
      MakeTestPrecondition(precond1, kTestId, kPrecondName));
  list.AddPrecondition(MakeTestPrecondition(precond2, kTestId2, kPrecondName2));
  list.AddPrecondition(MakeTestPrecondition(precond3, kTestId3, kPrecondName3));

  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());

  precond3->set_check_result_for_testing(kPrecondFailure3);
  // true, true, false
  EXPECT_EQ(CheckResult(kPrecondFailure3, precond3->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_check_result_for_testing(kPrecondFailure);
  // false, true, false
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_check_result_for_testing(kPrecondFailure2);
  // false, false, false
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, false
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond3->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, true
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_check_result_for_testing(FeaturePromoResult::Success());
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
      MakeTestPrecondition(precond2, kTestId2, kPrecondName2),
      MakeTestPrecondition(precond3, kTestId3, kPrecondName3));
  FeaturePromoPreconditionList list(
      MakeTestPrecondition(precond1, kTestId, kPrecondName));
  list.AppendAll(std::move(temp));

  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());

  precond3->set_check_result_for_testing(kPrecondFailure3);
  // true, true, false
  EXPECT_EQ(CheckResult(kPrecondFailure3, precond3->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_check_result_for_testing(kPrecondFailure);
  // false, true, false
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_check_result_for_testing(kPrecondFailure2);
  // false, false, false
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions());

  precond1->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, false
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond3->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, true
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions());

  precond2->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, true, true
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions());
}

TEST(FeaturePromoPreconditionTest,
     FeaturePromoPreconditionList_ExtractCachedData) {
  auto precond1 = std::make_unique<CachingFeaturePromoPrecondition>(
      kTestId, kPrecondName, FeaturePromoResult::Success());
  auto precond2 = std::make_unique<CachingFeaturePromoPrecondition>(
      kTestId2, kPrecondName2, FeaturePromoResult::Success());
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
