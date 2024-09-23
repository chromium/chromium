// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_store_migrator.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/mock_smart_bubble_stats_store.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::IsEmpty;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;
using testing::Unused;

constexpr char kTestHttpsURL[] = "https://example.org/path";
constexpr char kTestHost[] = "example.org";
constexpr char kTestHttpURL[] = "http://example.org/path";
constexpr char kTestSubdomainHttpURL[] = "http://login.example.org/path2";

// Creates a dummy http form with some basic arbitrary values.
PasswordForm CreateTestForm() {
  PasswordForm form;
  form.url = GURL(kTestHttpURL);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.action = GURL("https://example.org/action.html");
  form.username_value = u"user";
  form.password_value = u"password";
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

// Creates a dummy http PSL-matching form with some basic arbitrary values.
PasswordForm CreateTestPSLForm() {
  PasswordForm form;
  form.url = GURL(kTestSubdomainHttpURL);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.action = GURL(kTestSubdomainHttpURL);
  form.username_value = u"user2";
  form.password_value = u"password2";
  form.match_type = PasswordForm::MatchType::kPSL;
  return form;
}

// Creates an Android credential.
PasswordForm CreateAndroidCredential() {
  PasswordForm form;
  form.username_value = u"user3";
  form.password_value = u"password3";
  form.signon_realm = "android://hash@com.example.android/";
  form.url = GURL(form.signon_realm);
  form.action = GURL();
  form.match_type = PasswordForm::MatchType::kPSL;
  return form;
}

// Creates a local federated credential.
PasswordForm CreateLocalFederatedCredential() {
  PasswordForm form;
  form.username_value = u"user4";
  form.signon_realm = "federation://localhost/federation.example.com";
  form.url = GURL("http://localhost/");
  form.action = GURL("http://localhost/");
  form.federation_origin =
      url::SchemeHostPort(GURL("https://federation.example.com"));
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

class MockConsumer : public HttpPasswordStoreMigrator::Consumer {
 public:
  MOCK_METHOD(void,
              ProcessMigratedForms,
              (std::vector<std::unique_ptr<PasswordForm>>),
              (override));
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext() = default;
  ~MockNetworkContext() override = default;

  MOCK_METHOD(void,
              IsHSTSActiveForHost,
              (const std::string&, IsHSTSActiveForHostCallback),
              (override));
};

}  // namespace

class HttpPasswordStoreMigratorTest : public testing::Test {
 public:
  HttpPasswordStoreMigratorTest() = default;

  HttpPasswordStoreMigratorTest(const HttpPasswordStoreMigratorTest&) = delete;
  HttpPasswordStoreMigratorTest& operator=(
      const HttpPasswordStoreMigratorTest&) = delete;

  ~HttpPasswordStoreMigratorTest() override = default;

  MockConsumer& consumer() { return consumer_; }
  MockPasswordStoreInterface& store() { return *mock_store_; }
  password_manager::MockSmartBubbleStatsStore& smart_bubble_stats_store() {
    return mock_smart_bubble_stats_store_;
  }

  MockNetworkContext& mock_network_context() { return mock_network_context_; }

 protected:
  void TestEmptyStore(bool is_hsts);
  void TestFullStore(bool is_hsts);
  void TestMigratorDeletionByConsumer(bool is_hsts);

 private:
  base::test::TaskEnvironment task_environment_;
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStoreInterface> mock_store_ =
      base::MakeRefCounted<testing::StrictMock<MockPasswordStoreInterface>>();
  testing::NiceMock<MockNetworkContext> mock_network_context_;
  testing::NiceMock<MockSmartBubbleStatsStore> mock_smart_bubble_stats_store_;
};

void HttpPasswordStoreMigratorTest::TestEmptyStore(bool is_hsts) {
  PasswordFormDigest form_digest(CreateTestForm());
  form_digest.url = form_digest.url.DeprecatedGetOriginAsURL();
  EXPECT_CALL(store(), GetLogins(form_digest, _));
  EXPECT_CALL(mock_network_context(), IsHSTSActiveForHost(kTestHost, _))
      .Times(1)
      .WillOnce(testing::WithArg<1>(
          [is_hsts](auto cb) { std::move(cb).Run(is_hsts); }));

  EXPECT_CALL(store(), GetSmartBubbleStatsStore)
      .WillRepeatedly(Return(&smart_bubble_stats_store()));

  EXPECT_CALL(smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kTestHttpURL).DeprecatedGetOriginAsURL()))
      .Times(is_hsts);

  HttpPasswordStoreMigrator migrator(url::Origin::Create(GURL(kTestHttpsURL)),
                                     &store(), &mock_network_context(),
                                     &consumer());

  EXPECT_CALL(consumer(), ProcessMigratedForms(IsEmpty()));
  migrator.OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());
}

void HttpPasswordStoreMigratorTest::TestFullStore(bool is_hsts) {
  PasswordFormDigest form_digest(CreateTestForm());
  form_digest.url = form_digest.url.DeprecatedGetOriginAsURL();
  EXPECT_CALL(store(), GetLogins(form_digest, _));
  EXPECT_CALL(mock_network_context(), IsHSTSActiveForHost(kTestHost, _))
      .Times(1)
      .WillOnce(testing::WithArg<1>(
          [is_hsts](auto cb) { std::move(cb).Run(is_hsts); }));
  EXPECT_CALL(store(), GetSmartBubbleStatsStore)
      .WillRepeatedly(Return(&smart_bubble_stats_store()));
  EXPECT_CALL(smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kTestHttpURL).DeprecatedGetOriginAsURL()))
      .Times(is_hsts);
  HttpPasswordStoreMigrator migrator(url::Origin::Create(GURL(kTestHttpsURL)),
                                     &store(), &mock_network_context(),
                                     &consumer());

  PasswordForm form = CreateTestForm();
  PasswordForm psl_form = CreateTestPSLForm();
  PasswordForm android_form = CreateAndroidCredential();
  PasswordForm federated_form = CreateLocalFederatedCredential();
  PasswordForm expected_form = form;
  expected_form.url = GURL(kTestHttpsURL);
  expected_form.signon_realm =
      expected_form.url.DeprecatedGetOriginAsURL().spec();

  PasswordForm expected_federated_form = federated_form;
  expected_federated_form.url = GURL("https://localhost");
  expected_federated_form.action = GURL("https://localhost");

  EXPECT_CALL(store(), AddLogin(expected_form, _));
  EXPECT_CALL(store(), AddLogin(expected_federated_form, _));
  EXPECT_CALL(store(), RemoveLogin(_, form)).Times(is_hsts);
  EXPECT_CALL(store(), RemoveLogin(_, federated_form)).Times(is_hsts);
  EXPECT_CALL(consumer(),
              ProcessMigratedForms(ElementsAre(
                  Pointee(expected_form), Pointee(expected_federated_form))));
  std::vector<std::unique_ptr<PasswordForm>> results;
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
  // Setup expectations on store and network_context.
  EXPECT_CALL(store(), GetLogins(_, _));
  EXPECT_CALL(mock_network_context(), IsHSTSActiveForHost(kTestHost, _))
      .Times(1)
      .WillOnce(testing::WithArg<1>(
          [is_hsts](auto cb) { std::move(cb).Run(is_hsts); }));
  EXPECT_CALL(store(), GetSmartBubbleStatsStore)
      .WillRepeatedly(Return(&smart_bubble_stats_store()));

  EXPECT_CALL(smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kTestHttpURL).DeprecatedGetOriginAsURL()))
      .Times(is_hsts);
  // Construct the migrator, call |OnGetPasswordStoreResults| explicitly and
  // manually delete it.
  auto migrator = std::make_unique<HttpPasswordStoreMigrator>(
      url::Origin::Create(GURL(kTestHttpsURL)), &store(),
      &mock_network_context(), &consumer());

  EXPECT_CALL(consumer(), ProcessMigratedForms(_))
      .WillOnce(Invoke([&migrator](Unused) { migrator.reset(); }));

  migrator->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());
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
  const auto kOrigins = std::to_array<GURL>(
      {GURL("http://example.org/"), GURL("http://example.org/path/")});

  for (bool origin_has_paths : {true, false}) {
    PasswordForm http_html_form;
    http_html_form.url = kOrigins[origin_has_paths];
    http_html_form.signon_realm = "http://example.org/";
    http_html_form.scheme = PasswordForm::Scheme::kHtml;

    PasswordForm non_html_empty_realm_form;
    non_html_empty_realm_form.url = kOrigins[origin_has_paths];
    non_html_empty_realm_form.signon_realm = "http://example.org/";
    non_html_empty_realm_form.scheme = PasswordForm::Scheme::kBasic;

    PasswordForm non_html_form;
    non_html_form.url = kOrigins[origin_has_paths];
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
