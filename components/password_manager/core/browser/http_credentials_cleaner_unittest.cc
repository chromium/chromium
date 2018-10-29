// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_credentials_cleaner.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

enum class HttpCredentialType { kNoMatching, kEquivalent, kConflicting };

struct TestCase {
  bool is_hsts_enabled;
  autofill::PasswordForm::Scheme http_form_scheme;
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

    {true, autofill::PasswordForm::Scheme::SCHEME_HTML, false, true, true, true,
     HttpCredentialType::kNoMatching},
    {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, false, true, true,
     HttpCredentialType::kNoMatching},
    {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, false, true,
     HttpCredentialType::kNoMatching},
    {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true, false,
     HttpCredentialType::kConflicting},
    {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true, true,
     HttpCredentialType::kEquivalent},

    {false, autofill::PasswordForm::Scheme::SCHEME_HTML, false, true, true,
     true, HttpCredentialType::kNoMatching},
    {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, false, true,
     true, HttpCredentialType::kNoMatching},
    {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, false,
     true, HttpCredentialType::kNoMatching},
    {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true,
     false, HttpCredentialType::kConflicting},
    {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true, true,
     HttpCredentialType::kEquivalent},

    {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, false, true, true,
     true, HttpCredentialType::kNoMatching},
    {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, false, true,
     true, HttpCredentialType::kNoMatching},
    {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, false,
     true, HttpCredentialType::kNoMatching},
    {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
     false, HttpCredentialType::kConflicting},
    {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true, true,
     HttpCredentialType::kEquivalent},

    {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, false, true, true,
     true, HttpCredentialType::kNoMatching},
    {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, false, true,
     true, HttpCredentialType::kNoMatching},
    {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, false,
     true, HttpCredentialType::kNoMatching},
    {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
     false, HttpCredentialType::kConflicting},
    {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
     true, HttpCredentialType::kEquivalent}};

}  // namespace

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MockCredentialsCleanerObserver() = default;
  ~MockCredentialsCleanerObserver() override = default;
  MOCK_METHOD0(CleaningCompleted, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialsCleanerObserver);
};

class HttpCredentialCleanerTest : public ::testing::TestWithParam<TestCase> {
 public:
  HttpCredentialCleanerTest() = default;

  ~HttpCredentialCleanerTest() override = default;

 protected:
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();

  DISALLOW_COPY_AND_ASSIGN(HttpCredentialCleanerTest);
};

TEST_P(HttpCredentialCleanerTest, ReportHttpMigrationMetrics) {
  struct Histogram {
    bool test_hsts_enabled;
    HttpCredentialType test_type;
    std::string histogram_name;
  };

  static const std::string signon_realm[2] = {"https://example.org/realm/",
                                              "https://example.org/"};

  static const base::string16 username[2] = {base::ASCIIToUTF16("user0"),
                                             base::ASCIIToUTF16("user1")};

  static const base::string16 password[2] = {base::ASCIIToUTF16("pass0"),
                                             base::ASCIIToUTF16("pass1")};

  base::test::ScopedTaskEnvironment scoped_task_environment;
  ASSERT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));
  TestCase test = GetParam();
  SCOPED_TRACE(testing::Message()
               << "is_hsts_enabled=" << test.is_hsts_enabled
               << ", http_form_scheme="
               << static_cast<int>(test.http_form_scheme)
               << ", same_signon_realm=" << test.same_signon_realm
               << ", same_scheme=" << test.same_scheme
               << ", same_username=" << test.same_username
               << ", same_password=" << test.same_password);

  autofill::PasswordForm http_form;
  http_form.origin = GURL("http://example.org/");
  http_form.signon_realm = "http://example.org/";
  http_form.scheme = test.http_form_scheme;
  http_form.username_value = username[1];
  http_form.password_value = password[1];
  store_->AddLogin(http_form);

  autofill::PasswordForm https_form;
  https_form.origin = GURL("https://example.org/");
  https_form.signon_realm = signon_realm[test.same_signon_realm];
  https_form.username_value = username[test.same_username];
  https_form.password_value = password[test.same_password];
  https_form.scheme = test.http_form_scheme;
  if (!test.same_scheme) {
    https_form.scheme =
        (http_form.scheme == autofill::PasswordForm::Scheme::SCHEME_BASIC
             ? autofill::PasswordForm::Scheme::SCHEME_HTML
             : autofill::PasswordForm::Scheme::SCHEME_BASIC);
  }
  store_->AddLogin(https_form);

  auto request_context = base::MakeRefCounted<net::TestURLRequestContextGetter>(
      base::ThreadTaskRunnerHandle::Get());
  network::mojom::NetworkContextPtr network_context_pipe;
  auto network_context = std::make_unique<network::NetworkContext>(
      nullptr, mojo::MakeRequest(&network_context_pipe),
      request_context->GetURLRequestContext());

  if (test.is_hsts_enabled) {
    base::RunLoop run_loop;
    network_context->AddHSTS(http_form.origin.host(), base::Time::Max(),
                             false /*include_subdomains*/,
                             run_loop.QuitClosure());
    run_loop.Run();
  }
  scoped_task_environment.RunUntilIdle();

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
        // This needs to be network_context_pipe.get() and
        // not network_context.get() to make HSTS queries asynchronous, which
        // is what the progress tracking logic in HttpMetricsMigrationReporter
        // assumes.  This also matches reality, since
        // StoragePartition::GetNetworkContext will return a mojo pipe
        // even in the in-process case.
        return network_context_pipe.get();
      }),
      &prefs);
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);
  scoped_task_environment.RunUntilIdle();

  std::vector<Histogram> histograms_to_test;
  for (bool test_hsts_enabled : {true, false}) {
    std::string suffix =
        (test_hsts_enabled ? "WithHSTSEnabled" : "HSTSNotEnabled");
    histograms_to_test.push_back(
        {test_hsts_enabled, HttpCredentialType::kNoMatching,
         "PasswordManager.HttpCredentialsWithoutMatchingHttpsCredential." +
             suffix});
    histograms_to_test.push_back(
        {test_hsts_enabled, HttpCredentialType::kEquivalent,
         "PasswordManager.HttpCredentialsWithEquivalentHttpsCredential." +
             suffix});
    histograms_to_test.push_back(
        {test_hsts_enabled, HttpCredentialType::kConflicting,
         "PasswordManager.HttpCredentialsWithConflictingHttpsCredential." +
             suffix});
  }
  for (const auto& histogram : histograms_to_test) {
    int sample =
        static_cast<int>(histogram.test_type == test.expected &&
                         histogram.test_hsts_enabled == test.is_hsts_enabled);
    histogram_tester.ExpectUniqueSample(histogram.histogram_name, sample, 1);
  }

  const TestPasswordStore::PasswordMap current_store =
      store_->stored_passwords();
  if (test.is_hsts_enabled &&
      test.expected != HttpCredentialType::kConflicting) {
    // HTTP credentials have to be removed.
    EXPECT_TRUE(current_store.find(http_form.signon_realm)->second.empty());

    // For no matching case https credentials were added and for an equivalent
    // case they already existed.
    EXPECT_TRUE(base::ContainsKey(current_store, "https://example.org/"));
  } else {
    // Hsts not enabled or credentials are have different passwords, so
    // nothing should change in the password store.
    EXPECT_EQ(current_store, passwords_before_cleaning);
  }

  store_->ShutdownOnUIThread();
  scoped_task_environment.RunUntilIdle();
}

INSTANTIATE_TEST_CASE_P(,
                        HttpCredentialCleanerTest,
                        ::testing::ValuesIn(kCases));

TEST(HttpCredentialCleaner, StartCleanUpTest) {
  for (bool should_start_clean_up : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "should_start_clean_up=" << should_start_clean_up);

    base::test::ScopedTaskEnvironment scoped_task_environment;
    auto password_store = base::MakeRefCounted<TestPasswordStore>();
    ASSERT_TRUE(password_store->Init(syncer::SyncableService::StartSyncFlare(),
                                     nullptr));

    double last_time =
        (base::Time::Now() - base::TimeDelta::FromMinutes(10)).ToDoubleT();
    if (should_start_clean_up) {
      // Simulate that the clean-up was performed
      // (HttpCredentialCleaner::kCleanUpDelayInDays + 1) days ago.
      // We have to simulate this because the cleaning of obsolete HTTP
      // credentials is done with low frequency (with a delay of
      // |HttpCredentialCleaner::kCleanUpDelayInDays| days between two
      // clean-ups)
      last_time = (base::Time::Now() -
                   base::TimeDelta::FromDays(
                       HttpCredentialCleaner::kCleanUpDelayInDays + 1))
                      .ToDoubleT();
    }

    TestingPrefServiceSimple prefs;
    prefs.registry()->RegisterDoublePref(
        prefs::kLastTimeObsoleteHttpCredentialsRemoved, last_time);

    EXPECT_EQ(should_start_clean_up,
              HttpCredentialCleaner::ShouldRunCleanUp(&prefs));

    if (!should_start_clean_up) {
      password_store->ShutdownOnUIThread();
      scoped_task_environment.RunUntilIdle();
      continue;
    }

    auto request_context =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::ThreadTaskRunnerHandle::Get());
    network::mojom::NetworkContextPtr network_context_pipe;
    auto network_context = std::make_unique<network::NetworkContext>(
        nullptr, mojo::MakeRequest(&network_context_pipe),
        request_context->GetURLRequestContext());

    MockCredentialsCleanerObserver observer;
    HttpCredentialCleaner cleaner(
        password_store,
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          // This needs to be network_context_pipe.get() and
          // not network_context.get() to make HSTS queries asynchronous, which
          // is what the progress tracking logic in HttpMetricsMigrationReporter
          // assumes.  This also matches reality, since
          // StoragePartition::GetNetworkContext will return a mojo pipe
          // even in the in-process case.
          return network_context_pipe.get();
        }),
        &prefs);
    EXPECT_CALL(observer, CleaningCompleted);
    cleaner.StartCleaning(&observer);
    scoped_task_environment.RunUntilIdle();

    EXPECT_NE(prefs.GetDouble(prefs::kLastTimeObsoleteHttpCredentialsRemoved),
              last_time);

    password_store->ShutdownOnUIThread();
    scoped_task_environment.RunUntilIdle();
  }
}

}  // namespace password_manager
