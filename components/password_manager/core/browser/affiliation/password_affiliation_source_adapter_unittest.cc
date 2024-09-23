// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/affiliations/core/browser/mock_affiliation_source.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
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

#if !BUILDFLAG(IS_ANDROID)
constexpr char kTestWebFacetURIAlpha1[] = "https://one.alpha.example.com";
constexpr char kTestWebFacetURIAlpha2[] = "https://two.alpha.example.com";
#endif
constexpr char kTestAndroidFacetURIAlpha3[] =
    "android://hash@com.example.alpha.android";
constexpr char kTestWebRealmAlpha1[] = "https://one.alpha.example.com/";
constexpr char kTestWebRealmAlpha2[] = "https://two.alpha.example.com/";
constexpr char kTestAndroidRealmAlpha3[] =
    "android://hash@com.example.alpha.android/";

constexpr char kTestAndroidFacetURIBeta2[] =
    "android://hash@com.example.beta.android";
constexpr char kTestAndroidFacetURIBeta3[] =
    "android://hash@com.yetanother.beta.android";
constexpr char kTestAndroidRealmBeta2[] =
    "android://hash@com.example.beta.android/";
constexpr char kTestAndroidRealmBeta3[] =
    "android://hash@com.yetanother.beta.android/";

constexpr char kTestAndroidFacetURIGamma[] =
    "android://hash@com.example.gamma.android";
constexpr char kTestAndroidRealmGamma[] =
    "android://hash@com.example.gamma.android";

constexpr char16_t kTestUsername[] = u"JohnDoe";
constexpr char16_t kTestPassword[] = u"secret";

PasswordForm GetTestCredential(std::string_view signon_realm) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = signon_realm;
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  return form;
}

PasswordForm GetTestBlocklistedAndroidCredentials(
    std::string_view signon_realm) {
  PasswordForm form = GetTestCredential(signon_realm);
  form.blocked_by_user = true;
  form.username_value.clear();
  form.password_value.clear();
  return form;
}

}  // namespace

class PasswordAffiliationSourceAdapterTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_source_observer_ =
        std::make_unique<testing::StrictMock<MockAffiliationSourceObserver>>();
    password_store()->Init(/*prefs=*/nullptr,
                           /*affiliated_match_helper=*/nullptr);
    adapter_ = std::make_unique<PasswordAffiliationSourceAdapter>();
    adapter_->RegisterPasswordStore(password_store());
    RunUntilIdle();
  }

  void TearDown() override {
    adapter_.reset();
    if (password_store_) {
      DestroyPasswordStore();
    }
    mock_source_observer_.reset();
    // Clean up on the background thread.
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void AddLoginAndWait(const PasswordForm& form) {
    password_store()->AddLogin(form);
    RunUntilIdle();
  }

  void RemoveLoginAndWait(const PasswordForm& form) {
    password_store_->RemoveLogin(FROM_HERE, form);
    RunUntilIdle();
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

  void DestroyPasswordStore() {
    password_store_->ShutdownOnUIThread();
    password_store_ = nullptr;
  }

  TestPasswordStore* password_store() { return password_store_.get(); }

  affiliations::MockAffiliationSourceObserver* mock_source_observer() {
    return mock_source_observer_.get();
  }

  PasswordAffiliationSourceAdapter* adapter() { return adapter_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  std::unique_ptr<PasswordAffiliationSourceAdapter> adapter_;
  std::unique_ptr<MockAffiliationSourceObserver> mock_source_observer_;
};

// Verifies that no facets are returned if the password store is empty.
TEST_F(PasswordAffiliationSourceAdapterTest, GetFacetsReturnsNoCredentials) {
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
}

// Verifies that facets for web domain credentials are available via GetFacets.
TEST_F(PasswordAffiliationSourceAdapterTest, GetFacetsForWebOnlyCredentials) {
  AddLoginAndWait(GetTestCredential(kTestWebRealmAlpha1));
  AddLoginAndWait(GetTestCredential(kTestWebRealmAlpha2));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
       FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2)}));
#else
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
#endif
}

// Verifies that facets for Android application credentials are available
// via GetFacets.
TEST_F(PasswordAffiliationSourceAdapterTest,
       GetFacetsForAndroidOnlyCredentials) {
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmAlpha3));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmBeta2));
  AddLoginAndWait(GetTestBlocklistedAndroidCredentials(kTestAndroidRealmBeta3));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmGamma));

  std::vector<FacetURI> expected_facets = {
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma),
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3)};
  EXPECT_TRUE(ExpectAdapterToReturnFacets(expected_facets));
}

// Verifies that facets for Android application and web domain credentials
// are available via GetFacets.
TEST_F(PasswordAffiliationSourceAdapterTest,
       GetFacetsForAllTypesOfCredentials) {
  AddLoginAndWait(GetTestCredential(kTestWebRealmAlpha1));
  AddLoginAndWait(GetTestCredential(kTestWebRealmAlpha2));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmAlpha3));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmBeta2));
  AddLoginAndWait(GetTestBlocklistedAndroidCredentials(kTestAndroidRealmBeta3));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmGamma));

  std::vector<FacetURI> expected_facets = {
#if !BUILDFLAG(IS_ANDROID)
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2),
#endif
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma),
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3)};
  EXPECT_TRUE(ExpectAdapterToReturnFacets(expected_facets));
}

// Verifies that affiliations are not fetched if DisableSource is ever called.
TEST_F(PasswordAffiliationSourceAdapterTest, TestDisableSourceEmptyReturn) {
  AddLoginAndWait(GetTestCredential(kTestWebRealmAlpha1));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmAlpha3));
  adapter()->DisableSource();
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
}

// Verifies that the observer is not signaled that new domains have been added
// after DisableSourceing is called.
TEST_F(PasswordAffiliationSourceAdapterTest,
       TestDisableSourceDisablesObserving) {
  adapter()->StartObserving(mock_source_observer());
  adapter()->DisableSource();

  EXPECT_CALL(*mock_source_observer(), OnFacetsAdded).Times(0);
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmAlpha3));
}

// Verifies that the observer is signaled that new domains have been added after
// the adapter started observing the password store. It also tests that changes
// are no-op before StartObserving.
TEST_F(PasswordAffiliationSourceAdapterTest,
       SignalsCredentialsAddedAfterStartObserving) {
  EXPECT_CALL(*mock_source_observer(), OnFacetsAdded).Times(0);
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmBeta2));

  adapter()->StartObserving(mock_source_observer());

  EXPECT_CALL(*mock_source_observer(),
              OnFacetsAdded(ElementsAre(
                  FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3))));
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmAlpha3));
}

// Verifies that the observer is signaled that domains have been removed
// after the adapter started observing the password store.
TEST_F(PasswordAffiliationSourceAdapterTest,
       SignalsCredentialsRemovedAfterStartObserving) {
  EXPECT_CALL(*mock_source_observer(), OnFacetsAdded).Times(0);
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmBeta2));

  adapter()->StartObserving(mock_source_observer());

  EXPECT_CALL(*mock_source_observer(),
              OnFacetsRemoved(ElementsAre(
                  FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2))));
  RemoveLoginAndWait(GetTestCredential(kTestAndroidRealmBeta2));
}

// Verifies that updating a login's primary key triggers the observer, signaling
// a domain addition followed by a removal. This precise sequence prevents
// redundant affiliation requests.
TEST_F(PasswordAffiliationSourceAdapterTest,
       SignalsAddedBeforeRemovedForPrimaryKeyUpdates) {
  AddLoginAndWait(GetTestCredential(kTestAndroidRealmAlpha3));

  adapter()->StartObserving(mock_source_observer());
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*mock_source_observer(),
                OnFacetsAdded(ElementsAre(
                    FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3))));
    EXPECT_CALL(*mock_source_observer(),
                OnFacetsRemoved(ElementsAre(
                    FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3))));
  }

  PasswordForm old_form(GetTestCredential(kTestAndroidRealmAlpha3));
  PasswordForm new_form(old_form);
  new_form.username_value = u"NewUserName";
  password_store()->UpdateLoginWithPrimaryKey(new_form, old_form);
  RunUntilIdle();
}

// Adds and removes credentials for the same domain, and expects that the
// observer is signaled for added and removed domains on each repeated login.
TEST_F(PasswordAffiliationSourceAdapterTest,
       DuplicateCredentialsArePrefetchWithMultiplicity) {
  adapter()->StartObserving(mock_source_observer());
  std::vector<FacetURI> expected_facets;

  EXPECT_CALL(*mock_source_observer(),
              OnFacetsAdded(ElementsAre(
                  FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3))))
      .Times(3);

  PasswordForm form(GetTestCredential(kTestAndroidRealmAlpha3));
  AddLoginAndWait(form);

  PasswordForm form2(form);
  form2.username_value = u"JohnDoe2";
  AddLoginAndWait(form2);

  PasswordForm form3(form);
  form3.username_value = u"JohnDoe3";
  AddLoginAndWait(form3);

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
       FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
       FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3)}));

  EXPECT_CALL(*mock_source_observer(),
              OnFacetsRemoved(ElementsAre(
                  FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3))))
      .Times(3);
  RemoveLoginAndWait(form);
  RemoveLoginAndWait(form2);
  RemoveLoginAndWait(form3);
}

}  // namespace password_manager
