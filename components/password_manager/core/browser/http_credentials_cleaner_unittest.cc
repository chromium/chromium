// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_credentials_cleaner.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// The order of the enumerations needs to the reflect the order of the
// corresponding entries in the HttpCredentialCleaner::HttpCredentialType
// enumeration.
enum class HttpCredentialType { kConflicting, kEquivalent, kNoMatching };

struct TestCase {
  bool is_hsts_enabled;
  PasswordForm::Scheme http_form_scheme;
  bool same_signon_realm;
  bool same_scheme;
  bool same_username;
  bool same_password;
  // |expected| == kNoMatching if:
  // same_signon_realm & same_scheme & same_username = false
  // Otherwise:
  // |expected| == kEquivalent if:
  // same_signon_realm & same_scheme & same_username & same_password = true
  // Otherwise:
  // |expected| == kConflicting if:
  // same_signon_realm & same_scheme & same_username = true but same_password =
  // false
  HttpCredentialType expected;
};

constexpr static TestCase kCases[] = {

    {true, PasswordForm::Scheme::kHtml, false, true, true, true,
     HttpCredentialType::kNoMatching},
    {true, PasswordForm::Scheme::kHtml, true, false, true, true,
     HttpCredentialType::kNoMatching},
    {true, PasswordForm::Scheme::kHtml, true, true, false, true,
     HttpCredentialType::kNoMatching},
    {true, PasswordForm::Scheme::kHtml, true, true, true, false,
     HttpCredentialType::kConflicting},
    {true, PasswordForm::Scheme::kHtml, true, true, true, true,
     HttpCredentialType::kEquivalent},

    {false, PasswordForm::Scheme::kHtml, false, true, true, true,
     HttpCredentialType::kNoMatching},
    {false, PasswordForm::Scheme::kHtml, true, false, true, true,
     HttpCredentialType::kNoMatching},
    {false, PasswordForm::Scheme::kHtml, true, true, false, true,
     HttpCredentialType::kNoMatching},
    {false, PasswordForm::Scheme::kHtml, true, true, true, false,
     HttpCredentialType::kConflicting},
    {false, PasswordForm::Scheme::kHtml, true, true, true, true,
     HttpCredentialType::kEquivalent},

    {true, PasswordForm::Scheme::kBasic, false, true, true, true,
     HttpCredentialType::kNoMatching},
    {true, PasswordForm::Scheme::kBasic, true, false, true, true,
     HttpCredentialType::kNoMatching},
    {true, PasswordForm::Scheme::kBasic, true, true, false, true,
     HttpCredentialType::kNoMatching},
    {true, PasswordForm::Scheme::kBasic, true, true, true, false,
     HttpCredentialType::kConflicting},
    {true, PasswordForm::Scheme::kBasic, true, true, true, true,
     HttpCredentialType::kEquivalent},

    {false, PasswordForm::Scheme::kBasic, false, true, true, true,
     HttpCredentialType::kNoMatching},
    {false, PasswordForm::Scheme::kBasic, true, false, true, true,
     HttpCredentialType::kNoMatching},
    {false, PasswordForm::Scheme::kBasic, true, true, false, true,
     HttpCredentialType::kNoMatching},
    {false, PasswordForm::Scheme::kBasic, true, true, true, false,
     HttpCredentialType::kConflicting},
    {false, PasswordForm::Scheme::kBasic, true, true, true, true,
     HttpCredentialType::kEquivalent}};

}  // namespace

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MockCredentialsCleanerObserver() = default;

  MockCredentialsCleanerObserver(const MockCredentialsCleanerObserver&) =
      delete;
  MockCredentialsCleanerObserver& operator=(
      const MockCredentialsCleanerObserver&) = delete;

  ~MockCredentialsCleanerObserver() override = default;
  MOCK_METHOD0(CleaningCompleted, void());
};

class HttpCredentialCleanerTest : public ::testing::TestWithParam<TestCase> {
 public:
  HttpCredentialCleanerTest() = default;

  HttpCredentialCleanerTest(const HttpCredentialCleanerTest&) = delete;
  HttpCredentialCleanerTest& operator=(const HttpCredentialCleanerTest&) =
      delete;

  ~HttpCredentialCleanerTest() override = default;

 protected:
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
};

TEST_P(HttpCredentialCleanerTest, ReportHttpMigrationMetrics) {
  struct Histogram {
    bool test_hsts_enabled;
    HttpCredentialType test_type;
    std::string histogram_name;
  };

  static const std::array<std::string, 2> signon_realm = {
      "https://example.org/realm/", "https://example.org/"};

  static const std::array<std::u16string, 2> username = {u"user0", u"user1"};

  static const std::array<std::u16string, 2> password = {u"pass0", u"pass1"};

  base::test::TaskEnvironment task_environment;
  store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  TestCase test = GetParam();
  SCOPED_TRACE(testing::Message()
               << "is_hsts_enabled=" << test.is_hsts_enabled
               << ", http_form_scheme="
               << static_cast<int>(test.http_form_scheme)
               << ", same_signon_realm=" << test.same_signon_realm
               << ", same_scheme=" << test.same_scheme
               << ", same_username=" << test.same_username
               << ", same_password=" << test.same_password);

  PasswordForm http_form;
  http_form.url = GURL("http://example.org/");
  http_form.signon_realm = "http://example.org/";
  http_form.scheme = test.http_form_scheme;
  http_form.username_value = username[1];
  http_form.password_value = password[1];
  store_->AddLogin(http_form);

  PasswordForm https_form;
  https_form.url = GURL("https://example.org/");
  https_form.signon_realm = signon_realm[test.same_signon_realm];
  https_form.username_value = username[test.same_username];
  https_form.password_value = password[test.same_password];
  https_form.scheme = test.http_form_scheme;
  if (!test.same_scheme) {
    https_form.scheme = (http_form.scheme == PasswordForm::Scheme::kBasic
                             ? PasswordForm::Scheme::kHtml
                             : PasswordForm::Scheme::kBasic);
  }
  store_->AddLogin(https_form);

  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
  mojo::Remote<network::mojom::NetworkContext> network_context_remote;
  auto network_context = std::make_unique<network::NetworkContext>(
      nullptr, network_context_remote.BindNewPipeAndPassReceiver(),
      request_context.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>());

  if (test.is_hsts_enabled) {
    base::RunLoop run_loop;
    network_context->AddHSTS(http_form.url.host(), base::Time::Max(),
                             false /*include_subdomains*/,
                             run_loop.QuitClosure());
    run_loop.Run();
  }
  task_environment.RunUntilIdle();

  base::HistogramTester histogram_tester;
  const TestPasswordStore::PasswordMap passwords_before_cleaning =
      store_->stored_passwords();

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDoublePref(
      prefs::kLastTimeObsoleteHttpCredentialsRemoved, 0.0);

  MockCredentialsCleanerObserver observer;
  HttpCredentialCleaner cleaner(
      store_,
      base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
        // This needs to be network_context_remote.get() and
        // not network_context.get() to make HSTS queries asynchronous, which
        // is what the progress tracking logic in HttpMetricsMigrationReporter
        // assumes.  This also matches reality, since
        // StoragePartition::GetNetworkContext will return a mojo pipe
        // even in the in-process case.
        return network_context_remote.get();
      }),
      &prefs);
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);
  task_environment.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.HttpCredentials2",
      static_cast<HttpCredentialCleaner::HttpCredentialType>(
          static_cast<int>(test.expected) * 2 + test.is_hsts_enabled),
      1);

  const TestPasswordStore::PasswordMap current_store =
      store_->stored_passwords();
  if (test.is_hsts_enabled &&
      test.expected != HttpCredentialType::kConflicting) {
    // HTTP credentials have to be removed.
    EXPECT_FALSE(current_store.contains(http_form.signon_realm));

    // For no matching case https credentials were added and for an equivalent
    // case they already existed.
    EXPECT_TRUE(base::Contains(current_store, "https://example.org/"));
  } else {
    // Hsts not enabled or credentials are have different passwords, so
    // nothing should change in the password store.
    EXPECT_EQ(current_store, passwords_before_cleaning);
  }

  store_->ShutdownOnUIThread();
  task_environment.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         HttpCredentialCleanerTest,
                         ::testing::ValuesIn(kCases));

TEST(HttpCredentialCleaner, StartCleanUpTest) {
  for (bool should_start_clean_up : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "should_start_clean_up=" << should_start_clean_up);

    base::test::TaskEnvironment task_environment;
    auto password_store = base::MakeRefCounted<TestPasswordStore>();
    password_store->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

    double last_time =
        (base::Time::Now() - base::Minutes(10)).InSecondsFSinceUnixEpoch();
    if (should_start_clean_up) {
      // Simulate that the clean-up was performed
      // (HttpCredentialCleaner::kCleanUpDelayInDays + 1) days ago.
      // We have to simulate this because the cleaning of obsolete HTTP
      // credentials is done with low frequency (with a delay of
      // |HttpCredentialCleaner::kCleanUpDelayInDays| days between two
      // clean-ups)
      last_time = (base::Time::Now() -
                   base::Days(HttpCredentialCleaner::kCleanUpDelayInDays + 1))
                      .InSecondsFSinceUnixEpoch();
    }

    TestingPrefServiceSimple prefs;
    prefs.registry()->RegisterDoublePref(
        prefs::kLastTimeObsoleteHttpCredentialsRemoved, last_time);

    if (!should_start_clean_up) {
      password_store->ShutdownOnUIThread();
      task_environment.RunUntilIdle();
      continue;
    }

    auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
    mojo::Remote<network::mojom::NetworkContext> network_context_remote;
    auto network_context = std::make_unique<network::NetworkContext>(
        nullptr, network_context_remote.BindNewPipeAndPassReceiver(),
        request_context.get(),
        /*cors_exempt_header_list=*/std::vector<std::string>());

    MockCredentialsCleanerObserver observer;
    HttpCredentialCleaner cleaner(
        password_store,
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          // This needs to be network_context_remote.get() and
          // not network_context.get() to make HSTS queries asynchronous, which
          // is what the progress tracking logic in HttpMetricsMigrationReporter
          // assumes.  This also matches reality, since
          // StoragePartition::GetNetworkContext will return a mojo pipe
          // even in the in-process case.
          return network_context_remote.get();
        }),
        &prefs);
    EXPECT_TRUE(cleaner.NeedsCleaning());
    EXPECT_CALL(observer, CleaningCompleted);
    cleaner.StartCleaning(&observer);
    task_environment.RunUntilIdle();

    EXPECT_NE(prefs.GetDouble(prefs::kLastTimeObsoleteHttpCredentialsRemoved),
              last_time);

    password_store->ShutdownOnUIThread();
    task_environment.RunUntilIdle();
  }
}

}  // namespace password_manager
