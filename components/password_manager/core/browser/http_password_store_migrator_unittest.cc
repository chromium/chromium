// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_store_migrator.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using autofill::PasswordForm;
using testing::ElementsAre;
using testing::Invoke;
using testing::Pointee;
using testing::SaveArg;
using testing::Unused;
using testing::_;

constexpr char kTestHttpsURL[] = "https://example.org/path";
constexpr char kTestHttpURL[] = "http://example.org/path";
constexpr char kTestSubdomainHttpURL[] = "http://login.example.org/path2";

// Creates a dummy http form with some basic arbitrary values.
PasswordForm CreateTestForm() {
  PasswordForm form;
  form.origin = GURL(kTestHttpURL);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.action = GURL("https://example.org/action.html");
  form.username_value = base::ASCIIToUTF16("user");
  form.password_value = base::ASCIIToUTF16("password");
  return form;
}

// Creates a dummy http PSL-matching form with some basic arbitrary values.
PasswordForm CreateTestPSLForm() {
  PasswordForm form;
  form.origin = GURL(kTestSubdomainHttpURL);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.action = GURL(kTestSubdomainHttpURL);
  form.username_value = base::ASCIIToUTF16("user2");
  form.password_value = base::ASCIIToUTF16("password2");
  form.is_public_suffix_match = true;
  return form;
}

// Creates an Android credential.
PasswordForm CreateAndroidCredential() {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16("user3");
  form.password_value = base::ASCIIToUTF16("password3");
  form.signon_realm = "android://hash@com.example.android/";
  form.origin = GURL(form.signon_realm);
  form.action = GURL();
  form.is_affiliation_based_match = true;
  return form;
}

// Creates a local federated credential.
PasswordForm CreateLocalFederatedCredential() {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16("user4");
  form.signon_realm = "federation://localhost/federation.example.com";
  form.origin = GURL("http://localhost/");
  form.action = GURL("http://localhost/");
  form.federation_origin =
      url::Origin::Create(GURL("https://federation.example.com"));
  return form;
}

class MockConsumer : public HttpPasswordStoreMigrator::Consumer {
 public:
  MOCK_METHOD1(ProcessForms,
               void(const std::vector<autofill::PasswordForm*>& forms));

  void ProcessMigratedForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>> forms) override {
    std::vector<autofill::PasswordForm*> raw_forms(forms.size());
    std::transform(forms.begin(), forms.end(), raw_forms.begin(),
                   [](const std::unique_ptr<autofill::PasswordForm>& form) {
                     return form.get();
                   });
    ProcessForms(raw_forms);
  }
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(PasswordStore* store) : store_(store) {}

  // PasswordManagerClient:
  PasswordStore* GetProfilePasswordStore() const override { return store_; }
  void PostHSTSQueryForHost(const GURL& gurl,
                            HSTSCallback callback) const override {
    saved_callback_ = std::move(callback);
    PostHSTSQueryForHostHelper(gurl);
  }

  MOCK_CONST_METHOD1(PostHSTSQueryForHostHelper, void(const GURL&));
  HSTSCallback hsts_acquire_callback() { return std::move(saved_callback_); }

 private:
  PasswordStore* store_;
  mutable HSTSCallback saved_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

}  // namespace

class HttpPasswordStoreMigratorTest : public testing::Test {
 public:
  HttpPasswordStoreMigratorTest()
      : mock_store_(new testing::StrictMock<MockPasswordStore>),
        client_(mock_store_.get()) {
    mock_store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
  }

  ~HttpPasswordStoreMigratorTest() override {
    mock_store_->ShutdownOnUIThread();
  }

  MockConsumer& consumer() { return consumer_; }
  MockPasswordStore& store() { return *mock_store_; }
  MockPasswordManagerClient& client() { return client_; }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

 protected:
  void TestEmptyStore(bool is_hsts);
  void TestFullStore(bool is_hsts);
  void TestMigratorDeletionByConsumer(bool is_hsts);

 private:
  base::test::TaskEnvironment task_environment_;
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStore> mock_store_;
  MockPasswordManagerClient client_;

  DISALLOW_COPY_AND_ASSIGN(HttpPasswordStoreMigratorTest);
};

void HttpPasswordStoreMigratorTest::TestEmptyStore(bool is_hsts) {
  PasswordStore::FormDigest form(CreateTestForm());
  EXPECT_CALL(store(), GetLogins(form, _));
  EXPECT_CALL(client(), PostHSTSQueryForHostHelper(GURL(kTestHttpsURL)))
      .Times(1);
  HttpPasswordStoreMigrator migrator(GURL(kTestHttpsURL), &client(),
                                     &consumer());
  HSTSCallback callback = client().hsts_acquire_callback();
  std::move(callback).Run(is_hsts ? HSTSResult::kYes : HSTSResult::kNo);
  // We expect a potential call to |RemoveSiteStatsImpl| which is a async task
  // posted from |PasswordStore::RemoveSiteStats|. Hence the following lines are
  // necessary to ensure |RemoveSiteStatsImpl| gets called when expected.
  EXPECT_CALL(store(), RemoveSiteStatsImpl(GURL(kTestHttpURL).GetOrigin()))
      .Times(is_hsts);
  WaitForPasswordStore();

  EXPECT_CALL(consumer(), ProcessForms(std::vector<autofill::PasswordForm*>()));
  migrator.OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>>());
}

void HttpPasswordStoreMigratorTest::TestFullStore(bool is_hsts) {
  PasswordStore::FormDigest form_digest(CreateTestForm());
  EXPECT_CALL(store(), GetLogins(form_digest, _));
  EXPECT_CALL(client(), PostHSTSQueryForHostHelper(GURL(kTestHttpsURL)))
      .Times(1);
  HttpPasswordStoreMigrator migrator(GURL(kTestHttpsURL), &client(),
                                     &consumer());
  HSTSCallback callback = client().hsts_acquire_callback();
  std::move(callback).Run(is_hsts ? HSTSResult::kYes : HSTSResult::kNo);
  // We expect a potential call to |RemoveSiteStatsImpl| which is a async task
  // posted from |PasswordStore::RemoveSiteStats|. Hence the following lines are
  // necessary to ensure |RemoveSiteStatsImpl| gets called when expected.
  EXPECT_CALL(store(), RemoveSiteStatsImpl(GURL(kTestHttpURL).GetOrigin()))
      .Times(is_hsts);
  WaitForPasswordStore();

  PasswordForm form = CreateTestForm();
  PasswordForm psl_form = CreateTestPSLForm();
  PasswordForm android_form = CreateAndroidCredential();
  PasswordForm federated_form = CreateLocalFederatedCredential();
  PasswordForm expected_form = form;
  expected_form.origin = GURL(kTestHttpsURL);
  expected_form.signon_realm = expected_form.origin.GetOrigin().spec();

  PasswordForm expected_federated_form = federated_form;
  expected_federated_form.origin = GURL("https://localhost");
  expected_federated_form.action = GURL("https://localhost");

  EXPECT_CALL(store(), AddLogin(expected_form));
  EXPECT_CALL(store(), AddLogin(expected_federated_form));
  EXPECT_CALL(store(), RemoveLogin(form)).Times(is_hsts);
  EXPECT_CALL(store(), RemoveLogin(federated_form)).Times(is_hsts);
  EXPECT_CALL(consumer(),
              ProcessForms(ElementsAre(Pointee(expected_form),
                                       Pointee(expected_federated_form))));
  std::vector<std::unique_ptr<autofill::PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(psl_form));
  results.push_back(std::make_unique<PasswordForm>(form));
  results.push_back(std::make_unique<PasswordForm>(android_form));
  results.push_back(std::make_unique<PasswordForm>(federated_form));
  migrator.OnGetPasswordStoreResults(std::move(results));
}

// This test checks whether the migration successfully completes even if the
// migrator gets explicitly deleted by its consumer. This test will crash if
// this is not the case.
void HttpPasswordStoreMigratorTest::TestMigratorDeletionByConsumer(
    bool is_hsts) {
  // Setup expectations on store and client.
  EXPECT_CALL(store(), GetLogins(_, _));
  EXPECT_CALL(client(), PostHSTSQueryForHostHelper(GURL(kTestHttpsURL)))
      .Times(1);

  // Construct the migrator, call |OnGetPasswordStoreResults| explicitly and
  // manually delete it.
  auto migrator = std::make_unique<HttpPasswordStoreMigrator>(
      GURL(kTestHttpsURL), &client(), &consumer());
  migrator->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>>());
  EXPECT_CALL(consumer(), ProcessForms(_)).WillOnce(Invoke([&migrator](Unused) {
    migrator.reset();
  }));

  HSTSCallback callback = client().hsts_acquire_callback();
  std::move(callback).Run(is_hsts ? HSTSResult::kYes : HSTSResult::kNo);
  // We expect a potential call to |RemoveSiteStatsImpl| which is a async task
  // posted from |PasswordStore::RemoveSiteStats|. Hence the following lines are
  // necessary to ensure |RemoveSiteStatsImpl| gets called when expected.
  EXPECT_CALL(store(), RemoveSiteStatsImpl(GURL(kTestHttpURL).GetOrigin()))
      .Times(is_hsts);
  WaitForPasswordStore();
}

TEST_F(HttpPasswordStoreMigratorTest, EmptyStoreWithHSTS) {
  TestEmptyStore(true);
}

TEST_F(HttpPasswordStoreMigratorTest, EmptyStoreWithoutHSTS) {
  TestEmptyStore(false);
}

TEST_F(HttpPasswordStoreMigratorTest, FullStoreWithHSTS) {
  TestFullStore(true);
}

TEST_F(HttpPasswordStoreMigratorTest, FullStoreWithoutHSTS) {
  TestFullStore(false);
}

TEST_F(HttpPasswordStoreMigratorTest, MigratorDeletionByConsumerWithHSTS) {
  TestMigratorDeletionByConsumer(true);
}

TEST_F(HttpPasswordStoreMigratorTest, MigratorDeletionByConsumerWithoutHSTS) {
  TestMigratorDeletionByConsumer(false);
}

TEST(HttpPasswordStoreMigrator, MigrateHttpFormToHttpsTestSignonRealm) {
  const GURL kOrigins[] = {GURL("http://example.org/"),
                           GURL("http://example.org/path/")};

  for (bool origin_has_paths : {true, false}) {
    PasswordForm http_html_form;
    http_html_form.origin = kOrigins[origin_has_paths];
    http_html_form.signon_realm = "http://example.org/";
    http_html_form.scheme = PasswordForm::Scheme::kHtml;

    PasswordForm non_html_empty_realm_form;
    non_html_empty_realm_form.origin = kOrigins[origin_has_paths];
    non_html_empty_realm_form.signon_realm = "http://example.org/";
    non_html_empty_realm_form.scheme = PasswordForm::Scheme::kBasic;

    PasswordForm non_html_form;
    non_html_form.origin = kOrigins[origin_has_paths];
    non_html_form.signon_realm = "http://example.org/realm";
    non_html_form.scheme = PasswordForm::Scheme::kBasic;

    EXPECT_EQ(HttpPasswordStoreMigrator::MigrateHttpFormToHttps(http_html_form)
                  .signon_realm,
              "https://example.org/");
    EXPECT_EQ(HttpPasswordStoreMigrator::MigrateHttpFormToHttps(
                  non_html_empty_realm_form)
                  .signon_realm,
              "https://example.org/");
    EXPECT_EQ(HttpPasswordStoreMigrator::MigrateHttpFormToHttps(non_html_form)
                  .signon_realm,
              "https://example.org/realm");
  }
}

}  // namespace password_manager
