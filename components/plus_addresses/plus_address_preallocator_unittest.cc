// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include "base/json/values_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::SizeIs;

base::Value CreatePreallocatedPlusAddress(base::Time end_of_life) {
  return base::Value(
      base::Value::Dict().Set("eol", base::TimeToValue(end_of_life)));
}

}  // namespace

class PlusAddressPreallocatorTest : public ::testing::Test {
 public:
  PlusAddressPreallocatorTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    // Forward to make sure we can subtract from `base::Time::Now()` without
    // running into negative values.
    task_environment_.FastForwardBy(base::Days(100));
  }

 protected:
  MockPlusAddressHttpClient& http_client() { return http_client_; }
  PrefService& pref_service() { return pref_service_; }

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

  PlusAddressPreallocator allocator(&pref_service(), &http_client());

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

  PlusAddressPreallocator allocator(&pref_service(), &http_client());

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

  PlusAddressPreallocator allocator(&pref_service(), &http_client());

  EXPECT_THAT(GetPreallocatedAddresses(), SizeIs(3));
  EXPECT_EQ(GetPreallocatedAddressesNext(), 1);
}

}  // namespace plus_addresses
