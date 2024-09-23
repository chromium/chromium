// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/passkey_affiliation_source_adapter.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/rand_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/mock_affiliation_source.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
using ::affiliations::FacetURI;
using ::affiliations::MockAffiliationSourceObserver;
using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAreArray;

constexpr char kTestRpIdFacetURIAlpha1[] = "one.alpha.example.com";
constexpr char kTestRpIdFacetURIAlpha2[] = "two.alpha.example.com";
constexpr char kTestWebFacetURIAlpha1[] = "https://one.alpha.example.com";
constexpr char kTestWebFacetURIAlpha2[] = "https://two.alpha.example.com";
constexpr char kTestExtensionRpId[] = "chrome-extension://test-extension-id";

sync_pb::WebauthnCredentialSpecifics GetTestPasskey(const char* rp_id) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_rp_id(rp_id);
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_sync_id(base::RandBytesAsString(16));
  return passkey;
}
}  // namespace

class PasskeyAffiliationSourceAdapterTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_source_observer_ =
        std::make_unique<testing::StrictMock<MockAffiliationSourceObserver>>();
    adapter_ =
        std::make_unique<PasskeyAffiliationSourceAdapter>(test_passkey_model());
  }

  testing::AssertionResult ExpectAdapterToReturnFacets(
      const std::vector<FacetURI>& expected_facets) {
    base::MockCallback<affiliations::AffiliationSource::ResultCallback>
        callback;
    int calls = 0;
    ON_CALL(callback, Run(UnorderedElementsAreArray(expected_facets)))
        .WillByDefault([&] { ++calls; });
    adapter_->GetFacets(callback.Get());
    RunUntilIdle();
    return calls == 1 ? AssertionSuccess()
                      : (AssertionFailure() << "Error fetching facets.");
  }

  webauthn::TestPasskeyModel* test_passkey_model() {
    return test_passkey_model_.get();
  }

  affiliations::MockAffiliationSourceObserver* mock_source_observer() {
    return mock_source_observer_.get();
  }

  PasskeyAffiliationSourceAdapter* adapter() { return adapter_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockAffiliationSourceObserver> mock_source_observer_;
  std::unique_ptr<webauthn::TestPasskeyModel> test_passkey_model_ =
      std::make_unique<webauthn::TestPasskeyModel>();
  std::unique_ptr<PasskeyAffiliationSourceAdapter> adapter_;
};

// Verifies that no facets are returned when no passkeys are registered.
TEST_F(PasskeyAffiliationSourceAdapterTest, TestGetFacetsEmpty) {
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
}

// Verifies that facets for passkeys are available via GetFacets.
TEST_F(PasskeyAffiliationSourceAdapterTest, TestGetFacets) {
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha1));
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha2));

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
       FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2)}));
}

// Verifies that invalid facets are not returned by GetFacets.
TEST_F(PasskeyAffiliationSourceAdapterTest, TestInvalidFacetsIgnored) {
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha1));
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestExtensionRpId));

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)}));
}

// Verifies that the observer is signaled that new domains have been added after
// the adapter started observing the passkey model. It also tests that changes
// are no-op before StartObserving.
TEST_F(PasskeyAffiliationSourceAdapterTest, TestNewPasskeyDownloaded) {
  EXPECT_CALL(*mock_source_observer(), OnFacetsAdded).Times(0);
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha1));
  RunUntilIdle();

  adapter()->StartObserving(mock_source_observer());

  EXPECT_CALL(*mock_source_observer(),
              OnFacetsAdded(ElementsAre(
                  FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2))));
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha2));
  RunUntilIdle();
}

// Verifies that the observer is signaled that domains have been removed
// after the adapter started observing the passkey model.
TEST_F(PasskeyAffiliationSourceAdapterTest, TestPasskeyDeleted) {
  sync_pb::WebauthnCredentialSpecifics passkey =
      GetTestPasskey(kTestRpIdFacetURIAlpha1);
  test_passkey_model()->AddNewPasskeyForTesting(passkey);
  RunUntilIdle();

  adapter()->StartObserving(mock_source_observer());

  EXPECT_CALL(*mock_source_observer(),
              OnFacetsRemoved(ElementsAre(
                  FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1))));
  test_passkey_model()->DeletePasskey(passkey.credential_id(), FROM_HERE);
  RunUntilIdle();
}

}  // namespace password_manager
