// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include "base/json/values_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

using base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

base::Value CreatePreallocatedPlusAddress(base::Time end_of_life,
                                          std::string address = "") {
  return base::Value(
      base::Value::Dict()
          .Set(PlusAddressPreallocator::kEndOfLifeKey,
               base::TimeToValue(end_of_life))
          .Set(PlusAddressPreallocator::kPlusAddressKey, std::move(address)));
}

MATCHER_P2(IsPreallocatedPlusAddress, end_of_life, address, "") {
  if (!arg.is_dict()) {
    return false;
  }
  const base::Value::Dict& d = arg.GetDict();
  const std::string* plus_address =
      d.FindString(PlusAddressPreallocator::kPlusAddressKey);
  return plus_address && *plus_address == address &&
         base::ValueToTime(d.Find(PlusAddressPreallocator::kEndOfLifeKey)) ==
             end_of_life;
}

}  // namespace

class PlusAddressPreallocatorTest : public ::testing::Test {
 public:
  PlusAddressPreallocatorTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    // By default, assume that the notice has been accepted and plus addresses
    // are enabled.
    setting_service().set_has_accepted_notice(true);
    setting_service().set_is_plus_addresses_enabled(true);
    // Forward to make sure we can subtract from `base::Time::Now()` without
    // running into negative values.
    task_environment_.FastForwardBy(base::Days(100));
  }

 protected:
  MockPlusAddressHttpClient& http_client() { return http_client_; }
  PrefService& pref_service() { return pref_service_; }
  FakePlusAddressSettingService& setting_service() { return setting_service_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  const base::Value::List& GetPreallocatedAddresses() {
    return pref_service_.GetList(prefs::kPreallocatedAddresses);
  }
  void SetPreallocatedAddresses(base::Value::List addresses) {
    pref_service_.SetList(prefs::kPreallocatedAddresses, std::move(addresses));
  }

  int GetPreallocatedAddressesNext() {
    return pref_service_.GetInteger(prefs::kPreallocatedAddressesNext);
  }
  void SetPreallocatedAddressesNext(int next_index) {
    pref_service_.SetInteger(prefs::kPreallocatedAddressesNext, next_index);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;

  NiceMock<MockPlusAddressHttpClient> http_client_;
  FakePlusAddressSettingService setting_service_;
};

// Tests that plus addresses with an end of life in the future are not pruned on
// creation of the `PlusAddressPreallocator`.
TEST_F(PlusAddressPreallocatorTest,
       PrunePreallocatedPlusAddressesWithEolInFuture) {
  SetPreallocatedAddresses(base::Value::List()
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(1)))
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(2))));
  SetPreallocatedAddressesNext(1);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client());

  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(2));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 1);
}

// Tests that plus addresses with an end of life in the past are pruned on
// creation of the `PlusAddressPreallocator` and the index of the next plus
// address is set to 0 if no entries remain.
TEST_F(PlusAddressPreallocatorTest,
       PrunePreallocatedPlusAddressesWithEolInPast) {
  SetPreallocatedAddresses(base::Value::List()
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() - base::Days(1)))
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() - base::Days(2))));
  SetPreallocatedAddressesNext(1);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client());

  EXPECT_THAT(GetPreallocatedAddresses(), IsEmpty());
  EXPECT_EQ(GetPreallocatedAddressesNext(), 0);
}

// Tests that plus addresses with an end of life in the past are pruned on
// creation of the `PlusAddressPreallocator` and the index of the next plus
// address is in bounds.
TEST_F(PlusAddressPreallocatorTest,
       PrunePreallocatedPlusAddressesWithMixedEols) {
  SetPreallocatedAddresses(
      base::Value::List()
          .Append(
              CreatePreallocatedPlusAddress(base::Time::Now() - base::Days(1)))
          .Append(
              CreatePreallocatedPlusAddress(base::Time::Now() - base::Days(2)))
          .Append(
              CreatePreallocatedPlusAddress(base::Time::Now() + base::Days(2)))
          .Append(
              CreatePreallocatedPlusAddress(base::Time::Now() + base::Days(3)))
          .Append(CreatePreallocatedPlusAddress(base::Time::Now() +
                                                base::Days(4))));
  SetPreallocatedAddressesNext(4);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client());

  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(3));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 1);
}

// Tests that preallocated plus addresses are requested on startup if there are
// fewer than the minimum size present.
TEST_F(PlusAddressPreallocatorTest, RequestPreallocatedAddressesOnStartup) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "10"}});
  SetPreallocatedAddresses(base::Value::List().Append(
      CreatePreallocatedPlusAddress(base::Time::Now() + base::Days(1))));

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(
            PlusAddressHttpClient::PreallocatePlusAddressesResult(
                {PlusAddressHttpClient::PreallocatedPlusAddress{
                     .plus_address = "plus@plus.com",
                     .lifetime = base::Days(1)},
                 PlusAddressHttpClient::PreallocatedPlusAddress{
                     .plus_address = "plus2@plus.com",
                     .lifetime = base::Days(3)}})));
    EXPECT_CALL(check, Call);
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client());
  check.Call();
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
  check.Call();
  EXPECT_THAT(GetPreallocatedAddresses(),
              UnorderedElementsAre(
                  _,
                  IsPreallocatedPlusAddress(base::Time::Now() + base::Days(1),
                                            "plus@plus.com"),
                  IsPreallocatedPlusAddress(base::Time::Now() + base::Days(3),
                                            "plus2@plus.com")));
}

// Tests that no addresses are requested on startup if there are already enough
// present.
TEST_F(PlusAddressPreallocatorTest,
       DoNotRequestPreallocatedAddressesOnStartup) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "1"}});
  SetPreallocatedAddresses(base::Value::List().Append(
      CreatePreallocatedPlusAddress(base::Time::Now() + base::Days(1))));

  EXPECT_CALL(http_client(), PreallocatePlusAddresses).Times(0);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
}

// Tests that errors returned from the preallocation call are handled.
TEST_F(PlusAddressPreallocatorTest, HandleNetworkError) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "10"}});
  SetPreallocatedAddresses(base::Value::List().Append(
      CreatePreallocatedPlusAddress(base::Time::Now() + base::Days(1))));

  MockFunction<void()> check;
  {
    InSequence s;
    PlusAddressHttpClient::PreallocatePlusAddressesResult result =
        base::unexpected(
            PlusAddressRequestError::AsNetworkError(net::HTTP_NOT_FOUND));
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(result));
    EXPECT_CALL(check, Call);
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
  check.Call();
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(1));
}

}  // namespace plus_addresses
