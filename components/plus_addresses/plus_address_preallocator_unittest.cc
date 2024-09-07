// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/values_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

using base::test::RunOnceCallback;
using test::CreatePreallocatedPlusAddress;
using ::testing::_;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

PlusProfileOrError PlusProfileFromPreallocatedAddress(
    const url::Origin& origin,
    std::string plus_address) {
  return PlusProfileOrError(PlusProfile(
      /*profile_id=*/std::nullopt,
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          origin.GetURL().spec()),
      PlusAddress(std::move(plus_address)), /*is_confirmed=*/false));
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

base::RepeatingCallback<bool()> AlwaysEnabled() {
  return base::BindRepeating([]() { return true; });
}

base::RepeatingCallback<bool()> NeverEnabled() {
  return base::BindRepeating([]() { return false; });
}

class PlusAddressPreallocatorTest : public ::testing::Test {
 public:
  PlusAddressPreallocatorTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPlusAddressUserOnboardingEnabled,
                              features::kPlusAddressGlobalToggle},
        /*disabled_features=*/{});

    // By default, assume that the notice has been accepted and plus
    // addresses are enabled.
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
  base::test::ScopedFeatureList feature_list_;
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
                                    &http_client(), AlwaysEnabled());

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
                                    &http_client(), AlwaysEnabled());

  EXPECT_THAT(GetPreallocatedAddresses(), IsEmpty());
  EXPECT_EQ(GetPreallocatedAddressesNext(), 0);
}

// Tests that an invalid index for the next pre-allocated plus address is fixed
// during pruning.
TEST_F(PlusAddressPreallocatorTest,
       PrunePreallocatedPlusAddressesFixesNextIndex) {
  SetPreallocatedAddressesNext(-10);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
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
                                    &http_client(), AlwaysEnabled());

  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(3));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 1);
}

// Tests that an empty cache means a synchronous allocation is not successful.
TEST_F(PlusAddressPreallocatorTest, SynchronousAllocationNoPlusAddresses) {
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  EXPECT_EQ(allocator.AllocatePlusAddressSynchronously(
                url::Origin::Create(GURL("https://foo.com")),
                PlusAddressAllocator::AllocationMode::kAny),
            std::nullopt);
}

// Tests that a cache of only outdated plus addresses means that a synchronous
// allocation is not possible
TEST_F(PlusAddressPreallocatorTest, SychronousAllocationOutdatedPlusAddresses) {
  SetPreallocatedAddresses(base::Value::List()
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() - base::Days(1)))
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() - base::Days(2))));
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  EXPECT_EQ(allocator.AllocatePlusAddressSynchronously(
                url::Origin::Create(GURL("https://foo.com")),
                PlusAddressAllocator::AllocationMode::kAny),
            std::nullopt);
}

// Tests that valid plus addresses in the cache means that a synchronous
// allocation is possible.
TEST_F(PlusAddressPreallocatorTest, SynchronousAllocationValidPlusAddresses) {
  SetPreallocatedAddresses(base::Value::List()
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(1)))
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(2))));
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  EXPECT_NE(allocator.AllocatePlusAddressSynchronously(
                url::Origin::Create(GURL("https://foo.com")),
                PlusAddressAllocator::AllocationMode::kAny),
            std::nullopt);
}

// Tests that no synchronous allocation is possible if plus addresses are not
// enabled.
TEST_F(PlusAddressPreallocatorTest, SynchronousAllocationNotEnabled) {
  SetPreallocatedAddresses(base::Value::List()
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(1)))
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(2))));
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), NeverEnabled());
  EXPECT_EQ(allocator.AllocatePlusAddressSynchronously(
                url::Origin::Create(GURL("https://foo.com")),
                PlusAddressAllocator::AllocationMode::kAny),
            std::nullopt);
}

// Tests that no synchronous allocation is possible if the facet is not valid
TEST_F(PlusAddressPreallocatorTest, SynchronousAllocationInvalidFacet) {
  SetPreallocatedAddresses(base::Value::List()
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(1)))
                               .Append(CreatePreallocatedPlusAddress(
                                   base::Time::Now() + base::Days(2))));
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  EXPECT_EQ(allocator.AllocatePlusAddressSynchronously(
                url::Origin(), PlusAddressAllocator::AllocationMode::kAny),
            std::nullopt);
}

// Tests that preallocated plus addresses are requested on startup if there are
// fewer than the minimum size present.
TEST_F(PlusAddressPreallocatorTest, RequestPreallocatedAddressesOnStartup) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "2"}});
  SetPreallocatedAddresses(base::Value::List().Append(
      CreatePreallocatedPlusAddress(base::Time::Now() + base::Days(1))));

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(
            PlusAddressHttpClient::PreallocatePlusAddressesResult(
                {PreallocatedPlusAddress(PlusAddress("plus@plus.com"),
                                         /*lifetime=*/base::Days(1)),
                 PreallocatedPlusAddress(PlusAddress("plus2@plus.com"),
                                         /*lifetime=*/base::Days(3))})));
    EXPECT_CALL(check, Call);
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
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

// Tests that no preallocated plus addresses are requested on startup if the
// "enabled check" returns false.
TEST_F(PlusAddressPreallocatorTest,
       DoNotRequestPreallocatedAddressesOnStartupWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "2"}});
  ASSERT_THAT(GetPreallocatedAddresses(), IsEmpty());

  EXPECT_CALL(http_client(), PreallocatePlusAddresses).Times(0);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(),
                                    /*is_enabled_check=*/NeverEnabled());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
}

// Tests that no preallocated plus addresses are requested on startup if the
// notice screen has not yet been accepted.
TEST_F(PlusAddressPreallocatorTest,
       DoNotRequestPreallocatedAddressesOnStartupWhenNoticeNotAccepted) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "2"}});
  ASSERT_THAT(GetPreallocatedAddresses(), IsEmpty());

  EXPECT_CALL(http_client(), PreallocatePlusAddresses).Times(0);

  setting_service().set_has_accepted_notice(false);
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
}

// Tests that no preallocated plus addresses are requested on startup if the
// global toggle is off.
TEST_F(PlusAddressPreallocatorTest,
       DoNotRequestPreallocatedAddressesOnStartupWhenGlobalToggleOff) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "2"}});
  ASSERT_THAT(GetPreallocatedAddresses(), IsEmpty());

  EXPECT_CALL(http_client(), PreallocatePlusAddresses).Times(0);

  setting_service().set_is_plus_addresses_enabled(false);
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
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
                                    &http_client(), AlwaysEnabled());
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
                                    &http_client(), AlwaysEnabled());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
  check.Call();
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(1));
}

// Tests that encountering timeout errors leads to retries at appropriate
// intervals.
TEST_F(PlusAddressPreallocatorTest, RetryOnTimeout) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "1"}});

  MockFunction<void()> check;
  {
    InSequence s;
    const PlusAddressHttpClient::PreallocatePlusAddressesResult not_found =
        base::unexpected(
            PlusAddressRequestError::AsNetworkError(net::HTTP_REQUEST_TIMEOUT));
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(not_found));
    // The first retry is immediate.
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(not_found));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(not_found));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(
            PlusAddressHttpClient::PreallocatePlusAddressesResult(
                {PreallocatedPlusAddress(PlusAddress("plus@plus.com"),
                                         base::Days(1))})));
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
  check.Call();

  // The next retry happens 700 - 1300 milliseconds later
  task_environment().FastForwardBy(base::Milliseconds(1300));
  check.Call();

  // And the one after that 1400 - 2600 milliseconds later.
  task_environment().FastForwardBy(base::Milliseconds(2600));
}

// Tests that the back-off time for failed retries is ignored if there is an
// explicit allocation call. This makes sure that if Chrome is started when
// offline, the client cannot end up in a situation where no additional
// pre-allocate calls are made even if the user is actively trying to create a
// plus address.
TEST_F(PlusAddressPreallocatorTest, NoBackoffPeriodForUserTriggeredRequests) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "1"}});

  MockFunction<void()> check;
  {
    InSequence s;
    const PlusAddressHttpClient::PreallocatePlusAddressesResult not_found =
        base::unexpected(
            PlusAddressRequestError::AsNetworkError(net::HTTP_REQUEST_TIMEOUT));
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(not_found));
    // The first retry is immediate.
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(not_found));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(not_found));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(RunOnceCallback<0>(
            PlusAddressHttpClient::PreallocatePlusAddressesResult(
                {PreallocatedPlusAddress(PlusAddress("plus@plus.com"),
                                         base::Days(1))})));
    EXPECT_CALL(check, Call);
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  task_environment().FastForwardBy(
      PlusAddressPreallocator::kDelayUntilServerRequestAfterStartup);
  check.Call();

  // The next retry happens 700 - 1300 milliseconds later
  task_environment().FastForwardBy(base::Milliseconds(1300));
  check.Call();

  // A call to allocate ignores back off times.
  allocator.AllocatePlusAddress(url::Origin::Create(GURL("https://foo.com")),
                                PlusAddressAllocator::AllocationMode::kAny,
                                base::DoNothing());
  // Test artifact - needed to trigger processing of pending tasks.
  task_environment().FastForwardBy(base::Milliseconds(1));
  check.Call();
}

// Tests that trying to allocate a plus address for an opaque origin results in
// an error.
TEST_F(PlusAddressPreallocatorTest, AllocatePlusAddressForOpaqueOrigin) {
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  base::MockCallback<PlusAddressRequestCallback> callback;
  EXPECT_CALL(callback,
              Run(PlusProfileOrError(base::unexpected(PlusAddressRequestError(
                  PlusAddressRequestErrorType::kInvalidOrigin)))));
  allocator.AllocatePlusAddress(url::Origin(),
                                PlusAddressAllocator::AllocationMode::kAny,
                                callback.Get());
}

// Tests that trying to allocate a plus address while the global toggle is off
// results in an error.
TEST_F(PlusAddressPreallocatorTest, AllocatePlusAddressWithToggleOff) {
  setting_service().set_is_plus_addresses_enabled(false);
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  base::MockCallback<PlusAddressRequestCallback> callback;
  EXPECT_CALL(callback,
              Run(PlusProfileOrError(base::unexpected(PlusAddressRequestError(
                  PlusAddressRequestErrorType::kUserSignedOut)))));
  allocator.AllocatePlusAddress(url::Origin::Create(GURL("https://foo.com")),
                                PlusAddressAllocator::AllocationMode::kAny,
                                callback.Get());
}

// Tests that trying to allocate a plus address while the `IsEnabledCheck` is
// false results in an error.
TEST_F(PlusAddressPreallocatorTest, AllocatePlusAddressWithServiceDisabled) {
  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), NeverEnabled());
  base::MockCallback<PlusAddressRequestCallback> callback;
  EXPECT_CALL(callback,
              Run(PlusProfileOrError(base::unexpected(PlusAddressRequestError(
                  PlusAddressRequestErrorType::kUserSignedOut)))));
  allocator.AllocatePlusAddress(url::Origin::Create(GURL("https://foo.com")),
                                PlusAddressAllocator::AllocationMode::kAny,
                                callback.Get());
}

// Tests that allocating plus addresses returns pre-allocated plus addresses
// from the pool and cycles through them.
TEST_F(PlusAddressPreallocatorTest, AllocatePlusAddress) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "1"}});
  const base::Time kFuture = base::Time::Now() + base::Days(1);
  const std::string kPlusAddress1 = "plus1@plus.com";
  const std::string kPlusAddress2 = "plus2@plus.com";
  SetPreallocatedAddresses(
      base::Value::List()
          .Append(CreatePreallocatedPlusAddress(kFuture, kPlusAddress1))
          .Append(CreatePreallocatedPlusAddress(kFuture, kPlusAddress2)));

  const url::Origin kValidOrigin1 =
      url::Origin::Create(GURL("https://foo.com"));
  const url::Origin kValidOrigin2 =
      url::Origin::Create(GURL("https://bar.com"));
  constexpr auto kMode = PlusAddressAllocator::AllocationMode::kAny;
  base::MockCallback<PlusAddressRequestCallback> callback1;
  base::MockCallback<PlusAddressRequestCallback> callback2;
  base::MockCallback<PlusAddressRequestCallback> callback3;
  base::MockCallback<PlusAddressRequestCallback> callback4;
  {
    InSequence s;
    EXPECT_CALL(callback1, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin1, kPlusAddress1)));
    EXPECT_CALL(callback2, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin2, kPlusAddress2)));
    EXPECT_CALL(callback3, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin2, kPlusAddress1)));
    EXPECT_CALL(callback4, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin1, kPlusAddress2)));
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  allocator.AllocatePlusAddress(kValidOrigin1, kMode, callback1.Get());
  allocator.AllocatePlusAddress(kValidOrigin2, kMode, callback2.Get());
  allocator.AllocatePlusAddress(kValidOrigin2, kMode, callback3.Get());
  allocator.AllocatePlusAddress(kValidOrigin1, kMode, callback4.Get());
}

// Tests that calling `AllocatePlusAddress` removes outdated pre-allocated
// addresses and requests new ones if the remaining ones are less than the
// minimum size of the pre-allocation pool. It also checks that there no
// additional server requests are made if one is already ongoing.
TEST_F(PlusAddressPreallocatorTest,
       AllocatePlusAddressWithPreallocationAfterSomeExpire) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "3"}});
  const base::Time kFuture = base::Time::Now() + base::Days(1);
  const base::Time kFarFuture = base::Time::Now() + base::Days(3);
  const std::string kPlusAddress1 = "plus1@plus.com";
  const std::string kPlusAddress2 = "plus2@plus.com";
  const std::string kPlusAddress3 = "plus3@plus.com";
  const std::string kPlusAddress4 = "plus4@plus.com";
  SetPreallocatedAddresses(
      base::Value::List()
          .Append(CreatePreallocatedPlusAddress(kFarFuture, kPlusAddress1))
          .Append(CreatePreallocatedPlusAddress(kFuture, kPlusAddress2))
          .Append(CreatePreallocatedPlusAddress(kFarFuture, kPlusAddress3)));

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  // No plus addresses are pruned on allocator creation...
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(3));
  task_environment().FastForwardBy(base::Days(2));
  // ... or in reaction to time passing.
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(3));

  MockFunction<void(std::string_view)> check;
  base::MockCallback<PlusAddressRequestCallback> callback1;
  base::MockCallback<PlusAddressRequestCallback> callback2;
  base::MockCallback<PlusAddressRequestCallback> callback3;
  base::MockCallback<PlusAddressRequestCallback> callback4;
  base::MockCallback<PlusAddressRequestCallback> callback5;
  const url::Origin kValidOrigin = url::Origin::Create(GURL("https://bar.com"));
  constexpr auto kMode = PlusAddressAllocator::AllocationMode::kAny;
  PlusAddressHttpClient::PreallocatePlusAddressesCallback preallocate_callback;
  {
    InSequence s;
    EXPECT_CALL(callback1, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress1)));
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(MoveArg(&preallocate_callback));
    EXPECT_CALL(check, Call("Allocation requested."));
    // Until the server responds, we cycle through the existing pre-allocated
    // addresses.
    EXPECT_CALL(callback2, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress3)));
    EXPECT_CALL(check, Call("Server response received."));
    EXPECT_CALL(callback3, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress1)));
    EXPECT_CALL(callback4, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress3)));
    EXPECT_CALL(callback5, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress4)));
  }

  // The allocation prunes the cache and requests more pre-allocated addresses.
  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback1.Get());
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(2));
  task_environment().RunUntilIdle();
  check.Call("Allocation requested.");
  ASSERT_TRUE(preallocate_callback);

  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback2.Get());
  std::move(preallocate_callback)
      .Run(PlusAddressHttpClient::PreallocatePlusAddressesResult(
          {PreallocatedPlusAddress(PlusAddress(kPlusAddress4),
                                   /*lifetime=*/base::Days(1))}));
  check.Call("Server response received.");
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(3));

  // Now the callbacks cycle through the remaining addresses.
  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback3.Get());
  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback4.Get());
  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback5.Get());
}

TEST_F(PlusAddressPreallocatorTest,
       AllocatePlusAddressWithPreallocationAfterAllExpire) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressPreallocation,
      {{features::kPlusAddressPreallocationMinimumSize.name, "2"}});
  const base::Time kFuture = base::Time::Now() + base::Days(1);
  const std::string kPlusAddress1 = "plus1@plus.com";
  const std::string kPlusAddress2 = "plus2@plus.com";
  const std::string kPlusAddress3 = "plus3@plus.com";
  const std::string kPlusAddress4 = "plus4@plus.com";
  SetPreallocatedAddresses(
      base::Value::List()
          .Append(CreatePreallocatedPlusAddress(kFuture, kPlusAddress1))
          .Append(CreatePreallocatedPlusAddress(kFuture, kPlusAddress2)));

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  task_environment().FastForwardBy(base::Days(2));
  // The outdated plus addresses still exist.
  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(2));

  MockFunction<void(std::string_view)> check;
  base::MockCallback<PlusAddressRequestCallback> callback1;
  base::MockCallback<PlusAddressRequestCallback> callback2;
  base::MockCallback<PlusAddressRequestCallback> callback3;
  const url::Origin kValidOrigin = url::Origin::Create(GURL("https://bar.com"));
  constexpr auto kMode = PlusAddressAllocator::AllocationMode::kAny;
  PlusAddressHttpClient::PreallocatePlusAddressesCallback preallocate_callback;
  {
    InSequence s;
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(MoveArg(&preallocate_callback));

    // Once the server response is sent, the callbacks get executed.
    EXPECT_CALL(check, Call("About to send server response."));
    EXPECT_CALL(callback1, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress3)));
    EXPECT_CALL(callback2, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress4)));
    EXPECT_CALL(check, Call("Server response sent."));
    EXPECT_CALL(callback3, Run(PlusProfileFromPreallocatedAddress(
                               kValidOrigin, kPlusAddress3)));
  }

  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback1.Get());
  task_environment().RunUntilIdle();
  ASSERT_TRUE(preallocate_callback);
  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback2.Get());

  check.Call("About to send server response.");
  std::move(preallocate_callback)
      .Run(PlusAddressHttpClient::PreallocatePlusAddressesResult(
          {PreallocatedPlusAddress(PlusAddress(kPlusAddress3),
                                   /*lifetime=*/base::Days(1)),
           PreallocatedPlusAddress(PlusAddress(kPlusAddress4),
                                   /*lifetime=*/base::Days(1))}));
  check.Call("Server response sent.");

  // Subsequent allocation requests are fulfilled directly.
  allocator.AllocatePlusAddress(kValidOrigin, kMode, callback3.Get());
}

// Tests that removing plus addresses works correctly.
TEST_F(PlusAddressPreallocatorTest, RemoveAllocatedPlusAddress) {
  const auto kPlusAddress1 = PlusAddress("plus1@plusfoo.com");
  const auto kPlusAddress2 = PlusAddress("plus2@plusbar.com");
  SetPreallocatedAddresses(
      base::Value::List()
          .Append(CreatePreallocatedPlusAddress(
              base::Time::Now() + base::Days(1), *kPlusAddress1))
          .Append(CreatePreallocatedPlusAddress(
              base::Time::Now() + base::Days(2), *kPlusAddress2)));
  SetPreallocatedAddressesNext(1);

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  ASSERT_THAT(GetPreallocatedAddresses(), SizeIs(2));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 1);
  allocator.RemoveAllocatedPlusAddress(kPlusAddress1);
  ASSERT_THAT(GetPreallocatedAddresses(), SizeIs(1));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 0);
  allocator.RemoveAllocatedPlusAddress(kPlusAddress1);
  ASSERT_THAT(GetPreallocatedAddresses(), SizeIs(1));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 0);
  allocator.RemoveAllocatedPlusAddress(kPlusAddress2);
  ASSERT_THAT(GetPreallocatedAddresses(), IsEmpty());
  EXPECT_EQ(GetPreallocatedAddressesNext(), 0);
}

// Tests that errors encountered during allocation are forward to pending
// allocation requests.
TEST_F(PlusAddressPreallocatorTest, ErrorDuringAllocationRequest) {
  ASSERT_THAT(GetPreallocatedAddresses(), IsEmpty());

  const url::Origin kValidOrigin1 =
      url::Origin::Create(GURL("https://foo.com"));
  const url::Origin kValidOrigin2 =
      url::Origin::Create(GURL("https://bar.com"));
  constexpr auto kMode = PlusAddressAllocator::AllocationMode::kAny;
  constexpr auto kNotFound = base::unexpected(
      PlusAddressRequestError::AsNetworkError(net::HTTP_NOT_FOUND));
  base::MockCallback<PlusAddressRequestCallback> callback1;
  base::MockCallback<PlusAddressRequestCallback> callback2;
  MockFunction<void()> check;
  PlusAddressHttpClient::PreallocatePlusAddressesCallback preallocate_callback;
  {
    InSequence s;
    EXPECT_CALL(http_client(), PreallocatePlusAddresses)
        .WillOnce(MoveArg(&preallocate_callback));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(callback1, Run(PlusProfileOrError(kNotFound)));
    EXPECT_CALL(callback2, Run(PlusProfileOrError(kNotFound)));
    EXPECT_CALL(check, Call);
  }

  PlusAddressPreallocator allocator(&pref_service(), &setting_service(),
                                    &http_client(), AlwaysEnabled());
  allocator.AllocatePlusAddress(kValidOrigin1, kMode, callback1.Get());
  // Work-around to deal with asynchronicity.
  task_environment().FastForwardBy(base::Milliseconds(1));
  ASSERT_TRUE(preallocate_callback);
  allocator.AllocatePlusAddress(kValidOrigin2, kMode, callback2.Get());
  // No error is returned until the server replies with an error.
  check.Call();
  std::move(preallocate_callback).Run(kNotFound);
  check.Call();
}

}  // namespace
}  // namespace plus_addresses
