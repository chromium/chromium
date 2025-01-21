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
using ComputedData = FeaturePromoPrecondition::ComputedData;

DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(int, kIntegerData);
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(std::string, kStringData);
}  // namespace

TEST(FeaturePromoPreconditionTest, SetAndGetCachedData) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  ComputedData data;
  precond1.InitCache(kIntegerData);
  precond1.InitCachedData(kStringData, "foo");
  EXPECT_EQ(0, precond1.GetCachedDataForComputation(data, kIntegerData));
  EXPECT_EQ("foo", precond1.GetCachedDataForComputation(data, kStringData));
  EXPECT_EQ(0, data.Get(kIntegerData));
  EXPECT_EQ("foo", data.Get(kStringData));
  precond1.GetCachedDataForComputation(data, kIntegerData) = 2;
  precond1.GetCachedDataForComputation(data, kStringData) = "3";
  EXPECT_EQ(2, precond1.GetCachedDataForComputation(data, kIntegerData));
  EXPECT_EQ("3", precond1.GetCachedDataForComputation(data, kStringData));
  EXPECT_EQ(2, data.Get(kIntegerData));
  EXPECT_EQ("3", data.Get(kStringData));
}

TEST(FeaturePromoPreconditionTest, SetAndGetCachedDataDifferentPreconditions) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  CachingFeaturePromoPrecondition precond2(kTestId2, kPrecondName2,
                                           kPrecondFailure2);
  ComputedData data;
  precond1.InitCache(kIntegerData);
  precond2.InitCachedData(kStringData, "foo");
  EXPECT_EQ(0, precond1.GetCachedDataForComputation(data, kIntegerData));
  EXPECT_EQ("foo", precond2.GetCachedDataForComputation(data, kStringData));
  EXPECT_EQ(0, data.Get(kIntegerData));
  EXPECT_EQ("foo", data.Get(kStringData));
  precond1.GetCachedDataForComputation(data, kIntegerData) = 2;
  precond2.GetCachedDataForComputation(data, kStringData) = "3";
  EXPECT_EQ(2, precond1.GetCachedDataForComputation(data, kIntegerData));
  EXPECT_EQ("3", precond2.GetCachedDataForComputation(data, kStringData));
  EXPECT_EQ(2, data.Get(kIntegerData));
  EXPECT_EQ("3", data.Get(kStringData));
}

TEST(FeaturePromoPreconditionTest, GetCachedDataCrashesIfDataNotPresent) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  ComputedData data;
  EXPECT_CHECK_DEATH(precond1.GetCachedDataForComputation(data, kIntegerData));
}

TEST(FeaturePromoPreconditionTest, GetCachedDataCrashesIfCacheCollision) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  precond1.InitCache(kIntegerData);
  ComputedData data;
  internal::TypedPreconditionData<int> kOtherData(kIntegerData, 1);
  data.Add(kIntegerData, kOtherData);
  EXPECT_CHECK_DEATH(precond1.GetCachedDataForComputation(data, kIntegerData));
}

TEST(FeaturePromoPreconditionTest, ExtractCachedData) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  internal::PreconditionData::Collection coll;

  // This should still be valid until `coll` is deleted.
  ComputedData data;
  precond1.InitCache(kIntegerData, kStringData);
  precond1.GetCachedDataForComputation(data, kIntegerData) = 2;
  precond1.GetCachedDataForComputation(data, kStringData) = "3";

  precond1.ExtractCachedData(coll);
  EXPECT_EQ(2, *internal::PreconditionData::Get(coll, kIntegerData));
  EXPECT_EQ("3", *internal::PreconditionData::Get(coll, kStringData));

  // Data isn't moved from its memory location.
  EXPECT_EQ(internal::PreconditionData::Get(coll, kIntegerData),
            &data.Get(kIntegerData));
  EXPECT_EQ(internal::PreconditionData::Get(coll, kStringData),
            &data.Get(kStringData));
}

TEST(FeaturePromoPreconditionTest, GetAfterExtractCachedDataFails) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  internal::PreconditionData::Collection coll;

  // This should still be valid until `coll` is deleted.
  ComputedData data;
  precond1.InitCache(kIntegerData, kStringData);
  precond1.GetCachedDataForComputation(data, kIntegerData) = 2;
  precond1.GetCachedDataForComputation(data, kStringData) = "3";

  precond1.ExtractCachedData(coll);
  ComputedData data2;
  EXPECT_CHECK_DEATH(precond1.GetCachedDataForComputation(data2, kIntegerData));
}

TEST(FeaturePromoPreconditionTest, CachingFeaturePromoPrecondition) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  ComputedData data;
  EXPECT_EQ(kTestId, precond1.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond1.CheckPrecondition(data));
  EXPECT_EQ(kPrecondName, precond1.GetDescription());

  precond1.set_check_result_for_testing(FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(), precond1.CheckPrecondition(data));
  precond1.set_check_result_for_testing(kPrecondFailure);
  EXPECT_EQ(kPrecondFailure, precond1.CheckPrecondition(data));

  CachingFeaturePromoPrecondition precond2(kTestId, kPrecondName,
                                           FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(), precond2.CheckPrecondition(data));
}

TEST(FeaturePromoPreconditionTest, CallbackFeaturePromoPrecondition) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingCallback<FeaturePromoResult()>,
                         callback);
  CallbackFeaturePromoPrecondition precond(kTestId, kPrecondName,
                                           callback.Get());
  EXPECT_EQ(kTestId, precond.GetIdentifier());
  EXPECT_EQ(kPrecondName, precond.GetDescription());

  ComputedData data;
  EXPECT_CALL(callback, Run)
      .WillOnce(testing::Return(FeaturePromoResult::Success()));
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition(data));

  EXPECT_CALL(callback, Run).WillOnce(testing::Return(kPrecondFailure));
  EXPECT_EQ(kPrecondFailure, precond.CheckPrecondition(data));
}

TEST(FeaturePromoPreconditionTest, CallbackFeaturePromoPreconditionWithData) {
  DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(int, kIntValueId);
  UNCALLED_MOCK_CALLBACK(
      base::RepeatingCallback<FeaturePromoResult(ComputedData&)>, callback);
  CallbackFeaturePromoPrecondition precond(kTestId, kPrecondName,
                                           callback.Get());
  internal::TypedPreconditionData<int> int_data(kIntValueId, 1);
  ComputedData data;
  data.Add(kIntValueId, int_data);
  EXPECT_EQ(kTestId, precond.GetIdentifier());
  EXPECT_EQ(kPrecondName, precond.GetDescription());

  EXPECT_CALL(callback, Run).WillOnce([kIntValueId](ComputedData& data) {
    EXPECT_EQ(1, data.Get(kIntValueId));
    return FeaturePromoResult::Success();
  });
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition(data));

  EXPECT_CALL(callback, Run).WillOnce(testing::Return(kPrecondFailure));
  EXPECT_EQ(kPrecondFailure, precond.CheckPrecondition(data));
}

TEST(FeaturePromoPreconditionTest, ForwardingFeaturePromoPrecondition) {
  CachingFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                           kPrecondFailure);
  ForwardingFeaturePromoPrecondition precond2(precond1);
  ComputedData data;
  EXPECT_EQ(kTestId, precond2.GetIdentifier());
  EXPECT_EQ(kPrecondFailure, precond2.CheckPrecondition(data));
  EXPECT_EQ(kPrecondName, precond2.GetDescription());

  precond1.set_check_result_for_testing(FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(), precond2.CheckPrecondition(data));
  precond1.set_check_result_for_testing(kPrecondFailure);
  EXPECT_EQ(kPrecondFailure, precond2.CheckPrecondition(data));
}

TEST(FeaturePromoPreconditionTest, ForwardingFeaturePromoPreconditionWithData) {
  UNCALLED_MOCK_CALLBACK(
      base::RepeatingCallback<FeaturePromoResult(ComputedData&)>, callback);
  CallbackFeaturePromoPrecondition precond1(kTestId, kPrecondName,
                                            callback.Get());
  ForwardingFeaturePromoPrecondition precond2(precond1);
  ComputedData data;
  EXPECT_EQ(kTestId, precond2.GetIdentifier());
  EXPECT_EQ(kPrecondName, precond2.GetDescription());
  EXPECT_CALL(callback, Run(testing::Ref(data)))
      .WillOnce(testing::Return(kPrecondFailure));
  EXPECT_EQ(kPrecondFailure, precond2.CheckPrecondition(data));
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

template <typename T>
class TestPreconditionWithData : public CachingFeaturePromoPrecondition {
 public:
  TestPreconditionWithData(FeaturePromoPrecondition::Identifier id,
                           std::string description,
                           FeaturePromoResult initial_state,
                           ui::TypedIdentifier<T> data_id,
                           T value)
      : CachingFeaturePromoPrecondition(id, description, initial_state),
        data_id_(data_id) {
    InitCachedData(data_id, value);
  }
  ~TestPreconditionWithData() override = default;

  FeaturePromoResult CheckPrecondition(ComputedData& data) const override {
    GetCachedDataForComputation(data, data_id_);
    return CachingFeaturePromoPrecondition::CheckPrecondition(data);
  }

 private:
  const ui::TypedIdentifier<T> data_id_;
};

}  // namespace

TEST(FeaturePromoPreconditionTest, FeaturePromoPreconditionList_ComputesData) {
  auto precond1 = std::make_unique<TestPreconditionWithData<int>>(
      kTestId, kPrecondName, FeaturePromoResult::Success(), kIntegerData, 2);
  auto precond2 = std::make_unique<TestPreconditionWithData<std::string>>(
      kTestId2, kPrecondName2, FeaturePromoResult::Success(), kStringData, "3");

  FeaturePromoPreconditionList list(std::move(precond1), std::move(precond2));
  ComputedData data;
  list.CheckPreconditions(data);

  EXPECT_EQ(2, data.Get(kIntegerData));
  EXPECT_EQ("3", data.Get(kStringData));
}

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
  ComputedData data;
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions(data));

  precond3->set_check_result_for_testing(kPrecondFailure3);
  // true, true, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure3, precond3->GetIdentifier()),
            list.CheckPreconditions(data));

  precond1->set_check_result_for_testing(kPrecondFailure);
  // false, true, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions(data));

  precond2->set_check_result_for_testing(kPrecondFailure2);
  // false, false, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions(data));

  precond1->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions(data));

  precond3->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, true
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions(data));

  precond2->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, true, true
  data.release_all_references();
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions(data));
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
  ComputedData data;
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions(data));

  precond3->set_check_result_for_testing(kPrecondFailure3);
  // true, true, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure3, precond3->GetIdentifier()),
            list.CheckPreconditions(data));

  precond1->set_check_result_for_testing(kPrecondFailure);
  // false, true, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions(data));

  precond2->set_check_result_for_testing(kPrecondFailure2);
  // false, false, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions(data));

  precond1->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions(data));

  precond3->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, true
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions(data));

  precond2->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, true, true
  data.release_all_references();
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions(data));
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
  ComputedData data;
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions(data));

  precond3->set_check_result_for_testing(kPrecondFailure3);
  // true, true, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure3, precond3->GetIdentifier()),
            list.CheckPreconditions(data));

  precond1->set_check_result_for_testing(kPrecondFailure);
  // false, true, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions(data));

  precond2->set_check_result_for_testing(kPrecondFailure2);
  // false, false, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure, precond1->GetIdentifier()),
            list.CheckPreconditions(data));

  precond1->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, false
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions(data));

  precond3->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, false, true
  data.release_all_references();
  EXPECT_EQ(CheckResult(kPrecondFailure2, precond2->GetIdentifier()),
            list.CheckPreconditions(data));

  precond2->set_check_result_for_testing(FeaturePromoResult::Success());
  // true, true, true
  data.release_all_references();
  EXPECT_EQ(CheckResult(FeaturePromoResult::Success(), {}),
            list.CheckPreconditions(data));
}

TEST(FeaturePromoPreconditionTest,
     FeaturePromoPreconditionList_ExtractCachedData) {
  auto precond1 = std::make_unique<CachingFeaturePromoPrecondition>(
      kTestId, kPrecondName, FeaturePromoResult::Success());
  auto precond2 = std::make_unique<CachingFeaturePromoPrecondition>(
      kTestId2, kPrecondName2, FeaturePromoResult::Success());
  precond1->InitCachedData(kIntegerData, 2);
  precond2->InitCachedData(kStringData, "3");

  FeaturePromoPreconditionList list(std::move(precond1), std::move(precond2));
  internal::PreconditionData::Collection coll;
  list.ExtractCachedData(coll);

  EXPECT_EQ(2, *internal::PreconditionData::Get(coll, kIntegerData));
  EXPECT_EQ("3", *internal::PreconditionData::Get(coll, kStringData));
}

}  // namespace user_education
