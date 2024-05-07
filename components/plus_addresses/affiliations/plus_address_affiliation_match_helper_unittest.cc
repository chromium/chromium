// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {
using ::affiliations::FacetURI;
using ::affiliations::MockAffiliationService;
using ::base::test::RunOnceCallback;
using ::testing::UnorderedElementsAreArray;
}  // namespace

class PlusAddressAffiliationMatchHelperTest : public testing::Test {
 public:
  PlusAddressAffiliationMatchHelperTest() {
    plus_address_service_ = std::make_unique<PlusAddressService>(
        /*identity_manager=*/nullptr,
        /*pref_service=*/nullptr,
        std::make_unique<testing::NiceMock<MockPlusAddressHttpClient>>(),
        /*webdata_service=*/nullptr);

    match_helper_ = std::make_unique<PlusAddressAffiliationMatchHelper>(
        plus_address_service(), mock_affiliation_service());
  }

  MockAffiliationService* mock_affiliation_service() {
    return &mock_affiliation_service_;
  }

  PlusAddressService* plus_address_service() {
    return plus_address_service_.get();
  }

  PlusAddressAffiliationMatchHelper* match_helper() {
    return match_helper_.get();
  }

 private:
  testing::StrictMock<MockAffiliationService> mock_affiliation_service_;
  std::unique_ptr<PlusAddressService> plus_address_service_;
  std::unique_ptr<PlusAddressAffiliationMatchHelper> match_helper_;
};

// Verifies that PSL extensions are cached within the match helper and a single
// affiliation service call is issued.
TEST_F(PlusAddressAffiliationMatchHelperTest, GetPSLExtensionsCachesResult) {
  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(pls_extensions));

  base::MockCallback<PlusAddressAffiliationMatchHelper::PSLExtensionCallback>
      callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAreArray(pls_extensions)));

  match_helper()->GetPSLExtensions(callback.Get());

  // Now affiliation service isn't called.
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions).Times(0);
  EXPECT_CALL(callback, Run(UnorderedElementsAreArray(pls_extensions)));
  match_helper()->GetPSLExtensions(callback.Get());
}

// Verifies that the match helper correctly handles simultaneous fetch requests
// for PSL extensions.
TEST_F(PlusAddressAffiliationMatchHelperTest,
       GetPSLExtensionsHandlesSimultaneousCalls) {
  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  base::OnceCallback<void(std::vector<std::string>)> first_callback;

  base::MockCallback<PlusAddressAffiliationMatchHelper::PSLExtensionCallback>
      callback1;
  base::MockCallback<PlusAddressAffiliationMatchHelper::PSLExtensionCallback>
      callback2;
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
        // Store the callback and wait until all the GetPSLExtensions calls are
        // made.
        .WillOnce(MoveArg<0>(&first_callback));

    EXPECT_CALL(callback1, Run(UnorderedElementsAreArray(pls_extensions)));
    EXPECT_CALL(callback2, Run(UnorderedElementsAreArray(pls_extensions)));
  }

  match_helper()->GetPSLExtensions(callback1.Get());
  match_helper()->GetPSLExtensions(callback2.Get());

  // After all `GetPSLExtensions` are made, resolve the affiliation service
  // callback and make sure all the calls are resolved.
  std::move(first_callback).Run(pls_extensions);
}

}  // namespace plus_addresses
