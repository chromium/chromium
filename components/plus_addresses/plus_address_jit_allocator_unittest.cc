// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_jit_allocator.h"

#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace plus_addresses {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Message;
using ::testing::NiceMock;

// Shorthands for common errors that the allocator can throw.
const PlusProfileOrError kNotSupportedError =
    base::unexpected(PlusAddressRequestError(
        PlusAddressRequestErrorType::kRequestNotSupportedError));
const PlusProfileOrError kMaxRefreshesReachedError = base::unexpected(
    PlusAddressRequestError(PlusAddressRequestErrorType::kMaxRefreshesReached));

url::Origin GetSampleOrigin1() {
  return url::Origin::Create(GURL("https://example1.org"));
}

url::Origin GetSampleOrigin2() {
  return url::Origin::Create(GURL("https://another-example.co.uk"));
}

class PlusAddressJitAllocatorRefreshTest : public ::testing::Test {
 public:
  PlusAddressJitAllocatorRefreshTest() : allocator_(&http_client_) {}

 protected:
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  PlusAddressJitAllocator& allocator() { return allocator_; }
  MockPlusAddressHttpClient& http_client() { return http_client_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  NiceMock<MockPlusAddressHttpClient> http_client_;
  PlusAddressJitAllocator allocator_;
};

// Tests that the allocator translates the `AllocationMode` properly into the
// `refresh` parameter of the client.
TEST_F(PlusAddressJitAllocatorRefreshTest, RefreshParameterPassedOn) {
  EXPECT_CALL(http_client(),
              ReservePlusAddress(GetSampleOrigin1(), /*refresh=*/false, _));
  EXPECT_CALL(http_client(),
              ReservePlusAddress(GetSampleOrigin1(), /*refresh=*/true, _));
  EXPECT_CALL(http_client(),
              ReservePlusAddress(GetSampleOrigin2(), /*refresh=*/false, _));

  allocator().AllocatePlusAddress(GetSampleOrigin1(),
                                  PlusAddressAllocator::AllocationMode::kAny,
                                  base::DoNothing());
  allocator().AllocatePlusAddress(
      GetSampleOrigin1(), PlusAddressAllocator::AllocationMode::kNewPlusAddress,
      base::DoNothing());
  allocator().AllocatePlusAddress(GetSampleOrigin2(),
                                  PlusAddressAllocator::AllocationMode::kAny,
                                  base::DoNothing());
}

TEST_F(PlusAddressJitAllocatorRefreshTest, AllocationIsNeverSynchronous) {
  EXPECT_EQ(allocator().AllocatePlusAddressSynchronously(
                GetSampleOrigin1(), PlusAddressAllocator::AllocationMode::kAny),
            std::nullopt);
}

// Tests that refreshing is only allowed `kMaxPlusAddressRefreshesPerOrigin`
// times per origin.
TEST_F(PlusAddressJitAllocatorRefreshTest, RefreshLimit) {
  // Note: In practice, this would be a different profile with each call - but
  // the test does not need to reproduce this level of fidelity.
  const PlusProfile kSampleProfile = test::CreatePlusProfile();
  ON_CALL(http_client(), ReservePlusAddress(_, /*refresh=*/true, _))
      .WillByDefault([&kSampleProfile](const url::Origin& origin, bool refresh,
                                       PlusAddressRequestCallback cb) {
        std::move(cb).Run(kSampleProfile);
      });

  for (int i = 0; i < PlusAddressAllocator::kMaxPlusAddressRefreshesPerOrigin;
       ++i) {
    SCOPED_TRACE(Message() << "Iteration #" << (i + 1));
    EXPECT_TRUE(allocator().IsRefreshingSupported(GetSampleOrigin1()));

    base::MockCallback<PlusAddressRequestCallback> callback;
    EXPECT_CALL(callback, Run(PlusProfileOrError(kSampleProfile)));
    allocator().AllocatePlusAddress(
        GetSampleOrigin1(),
        PlusAddressAllocator::AllocationMode::kNewPlusAddress, callback.Get());
  }

  EXPECT_FALSE(allocator().IsRefreshingSupported(GetSampleOrigin1()));
  {
    base::MockCallback<PlusAddressRequestCallback> callback;
    EXPECT_CALL(callback, Run(kMaxRefreshesReachedError));
    allocator().AllocatePlusAddress(
        GetSampleOrigin1(),
        PlusAddressAllocator::AllocationMode::kNewPlusAddress, callback.Get());
  }

  // However, refreshing addresses on a different origin still works.
  EXPECT_TRUE(allocator().IsRefreshingSupported(GetSampleOrigin2()));
  {
    base::MockCallback<PlusAddressRequestCallback> callback;
    EXPECT_CALL(callback, Run(PlusProfileOrError(kSampleProfile)));
    allocator().AllocatePlusAddress(
        GetSampleOrigin2(),
        PlusAddressAllocator::AllocationMode::kNewPlusAddress, callback.Get());
  }
}

// Tests that the allocator handles the server response for "refresh quota is
// exhausted" properly by disabling refresh for a cool down period.
TEST_F(PlusAddressJitAllocatorRefreshTest,
       HandleServerResponseForQuotaExhausted) {
  base::MockCallback<PlusAddressRequestCallback> callback;
  {
    InSequence s;
    PlusProfileOrError response = base::unexpected(
        PlusAddressRequestError::AsNetworkError(net::HTTP_TOO_MANY_REQUESTS));
    EXPECT_CALL(http_client(),
                ReservePlusAddress(GetSampleOrigin1(), /*refresh=*/true, _))
        .WillOnce(base::test::RunOnceCallback<2>(response));
    EXPECT_CALL(callback, Run(response));
  }

  EXPECT_TRUE(allocator().IsRefreshingSupported(GetSampleOrigin1()));
  allocator().AllocatePlusAddress(
      GetSampleOrigin1(), PlusAddressAllocator::AllocationMode::kNewPlusAddress,
      callback.Get());

  // After receiving the error, refreshing is no longer supported - regardless
  // of domain.
  EXPECT_FALSE(allocator().IsRefreshingSupported(GetSampleOrigin1()));
  EXPECT_FALSE(allocator().IsRefreshingSupported(GetSampleOrigin2()));

  // This effect persists...
  task_environment().FastForwardBy(base::Hours(6));
  EXPECT_FALSE(allocator().IsRefreshingSupported(GetSampleOrigin1()));
  EXPECT_FALSE(allocator().IsRefreshingSupported(GetSampleOrigin2()));

  // ... for 24 hours.
  task_environment().FastForwardBy(base::Hours(19));
  EXPECT_TRUE(allocator().IsRefreshingSupported(GetSampleOrigin1()));
  EXPECT_TRUE(allocator().IsRefreshingSupported(GetSampleOrigin2()));
}

}  // namespace
}  // namespace plus_addresses
