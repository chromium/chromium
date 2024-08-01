// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/android_metrics_service_client.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/persistent_histograms.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";

class TestClient : public AndroidMetricsServiceClient {
 public:
  TestClient()
      : sample_bucket_value_(0),
        sampled_in_rate_per_mille_(1000),
        package_name_rate_per_mille_(1000),
        record_package_name_for_app_type_(true) {}

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  ~TestClient() override = default;

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }

  void SetSampleRatePerMille(int per_mille) {
    sampled_in_rate_per_mille_ = per_mille;
  }

  void SetInSample(bool value) {
    sampled_in_rate_per_mille_ = value ? 1000 : 0;
  }

  void SetRecordPackageNameForAppType(bool value) {
    record_package_name_for_app_type_ = value;
  }

  void SetPackageNameSamplePerMille(int per_mille) {
    package_name_rate_per_mille_ = per_mille;
  }

  void SetInPackageNameSample(bool value) {
    package_name_rate_per_mille_ = value ? 1000 : 0;
  }

  void SetSampleBucketValue(int per_mille) { sample_bucket_value_ = per_mille; }

  // Expose the super class implementation for testing.
  using AndroidMetricsServiceClient::IsInSample;
  using AndroidMetricsServiceClient::ShouldRecordPackageName;

 protected:
  void OnMetricsStart() override {}

  void OnMetricsNotStarted() override {}

  int GetSampleBucketValue() const override { return sample_bucket_value_; }

  int GetSampleRatePerMille() const override {
    return sampled_in_rate_per_mille_;
  }

  bool CanRecordPackageNameForAppType() override {
    return record_package_name_for_app_type_;
  }

  // AndroidMetricsServiceClient:
  int32_t GetProduct() override {
    return metrics::ChromeUserMetricsExtension::CHROME;
  }

  int GetPackageNameLimitRatePerMille() override {
    return package_name_rate_per_mille_;
  }

  void RegisterAdditionalMetricsProviders(MetricsService* service) override {}

 private:
  int sample_bucket_value_;
  int sampled_in_rate_per_mille_;
  int package_name_rate_per_mille_;
  bool record_package_name_for_app_type_;
};

class SampleBucketValueTestClient : public AndroidMetricsServiceClient {
 public:
  SampleBucketValueTestClient() = default;

  SampleBucketValueTestClient(const SampleBucketValueTestClient&) = delete;
  SampleBucketValueTestClient& operator=(const SampleBucketValueTestClient&) =
      delete;

  ~SampleBucketValueTestClient() override = default;

  using AndroidMetricsServiceClient::GetSampleBucketValue;

 protected:
  // AndroidMetricsServiceClient:
  void OnMetricsStart() override {}
  void OnMetricsNotStarted() override {}
  int GetSampleRatePerMille() const override { return 0; }
  bool CanRecordPackageNameForAppType() override { return false; }
  int32_t GetProduct() override {
    return metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
  }
  int GetPackageNameLimitRatePerMille() override { return 0; }
  void RegisterAdditionalMetricsProviders(MetricsService* service) override {}
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  AndroidMetricsServiceClient::RegisterPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  return client;
}

}  // namespace

class AndroidMetricsServiceClientTest : public testing::Test {
 public:
  AndroidMetricsServiceClientTest()
      : test_begin_time_(base::Time::Now().ToTimeT()),
        task_runner_(new base::TestSimpleTaskRunner) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
    // Needed because RegisterMetricsProvidersAndInitState() checks for this.
    metrics::SubprocessMetricsProvider::CreateInstance();
  }

  AndroidMetricsServiceClientTest(const AndroidMetricsServiceClientTest&) =
      delete;
  AndroidMetricsServiceClientTest& operator=(
      const AndroidMetricsServiceClientTest&) = delete;

  const int64_t test_begin_time_;

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  ~AndroidMetricsServiceClientTest() override {
    // The global allocator has to be detached here so that no metrics created
    // by code called below get stored in it as that would make for potential
    // use-after-free operations if that code is called again.
    base::GlobalHistogramAllocator::ReleaseForTesting();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

// Verify that Chrome does not start watching for browser crashes before setting
// up field trials. For Android embedders, Chrome should not watch for crashes
// then because, at the time of field trial set-up, it is not possible to know
// whether the embedder will come to foreground. The embedder may remain in the
// background for the browser process lifetime, and in this case, Chrome should
// not watch for crashes so that exiting is not considered a crash. Embedders
// start watching for crashes when foregrounding via
// MetricsService::OnAppEnterForeground().
TEST_F(AndroidMetricsServiceClientTest,
       DoNotWatchForCrashesBeforeFieldTrialSetUp) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs.get());
  EXPECT_TRUE(client->metrics_state_manager()
                  ->clean_exit_beacon()
                  ->GetUserDataDirForTesting()
                  .empty());
  EXPECT_TRUE(client->metrics_state_manager()
                  ->clean_exit_beacon()
                  ->GetBeaconFilePathForTesting()
                  .empty());
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true, true);
  client->Initialize(prefs.get());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_TRUE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false, false);
  client->Initialize(prefs.get());
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_GE(prefs->GetInt64(prefs::kMetricsReportingEnabledTimestamp),
            test_begin_time_);
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kMetricsReportingEnabledTimestamp));
}

// If there is already a valid client ID and enabled date, they should be
// reused.
TEST_F(AndroidMetricsServiceClientTest,
       TestKeepExistingClientIdAndEnabledDate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  int64_t enabled_date = 12345;
  prefs->SetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp,
                  enabled_date);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(kTestClientId, prefs->GetString(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(enabled_date,
            prefs->GetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest,
       TestSetConsentFalseClearsIdAndEnabledDate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest,
       TestShouldNotUploadPackageName_AppType) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(false);
  client->SetInPackageNameSample(true);
  std::string package_name = client->GetAppPackageNameIfLoggable();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestShouldNotUploadPackageName_SampledOut) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  client->SetInPackageNameSample(false);
  std::string package_name = client->GetAppPackageNameIfLoggable();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AndroidMetricsServiceClientTest, TestCanUploadPackageName) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  client->SetInPackageNameSample(true);
  std::string package_name = client->GetAppPackageNameIfLoggable();
  EXPECT_FALSE(package_name.empty());
}

TEST_F(AndroidMetricsServiceClientTest, TestGetPackageNameInternal) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  // Make sure GetPackageName returns a non-empty string.
  EXPECT_FALSE(client->GetAppPackageName().empty());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestPackageNameLogic_SampleRateBelowPackageNameRate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetSampleRatePerMille(80);
  client->SetPackageNameSamplePerMille(100);

  // When GetSampleRatePerMille() <= 100, everything in-sample should also be in
  // the package name sample.
  for (int value = 0; value < 80; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_TRUE(client->IsInSample())
        << "Value " << value << " should be in-sample";
    EXPECT_TRUE(client->ShouldRecordPackageName())
        << "Value " << value << " should be in the package name sample";
  }
  // After this, the only thing we care about is that we're out of sample (the
  // package name logic shouldn't matter at this point, because we won't upload
  // any records).
  for (int value = 80; value < 1000; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_FALSE(client->IsInSample())
        << "Value " << value << " should be out of sample";
  }
}

TEST_F(AndroidMetricsServiceClientTest,
       TestPackageNameLogic_SampleRateAbovePackageNameRate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetSampleRatePerMille(900);
  client->SetPackageNameSamplePerMille(100);

  // When GetSampleRate() > 0.10, only values up to 0.10 should be in the
  // package name sample.
  for (int value = 0; value < 10; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_TRUE(client->IsInSample())
        << "Value " << value << " should be in-sample";
    EXPECT_TRUE(client->ShouldRecordPackageName())
        << "Value " << value << " should be in the package name sample";
  }
  // After this (but until we hit the sample rate), clients should be in sample
  // but not upload the package name.
  for (int value = 100; value < 900; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_TRUE(client->IsInSample())
        << "Value " << value << " should be in-sample";
    EXPECT_FALSE(client->ShouldRecordPackageName())
        << "Value " << value << " should be out of the package name sample";
  }
  // After this, the only thing we care about is that we're out of sample (the
  // package name logic shouldn't matter at this point, because we won't upload
  // any records).
  for (int value = 900; value < 1000; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_FALSE(client->IsInSample())
        << "Value " << value << " should be out of sample";
  }
}

TEST_F(AndroidMetricsServiceClientTest, TestCanForceEnableMetrics) {
  ForceEnableMetricsReportingForTesting();

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Flag should have higher precedence than sampling or user consent (but not
  // app consent, so we set that to 'true' for this case).
  client->SetHaveMetricsConsent(false, /* app_consent */ true);
  client->SetInSample(false);
  client->Initialize(prefs.get());

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestCanForceEnableMetricsIfAlreadyEnabled) {
  ForceEnableMetricsReportingForTesting();

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // This is a sanity check: flip consent and sampling to true, just to make
  // sure the flag continues to work.
  client->SetHaveMetricsConsent(true, true);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestCannotForceEnableMetricsIfAppOptsOut) {
  ForceEnableMetricsReportingForTesting();

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Even with the flag, app consent should be respected.
  client->SetHaveMetricsConsent(true, /* app_consent */ false);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  EXPECT_FALSE(client->IsReportingEnabled());
  EXPECT_FALSE(client->IsRecordingActive());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestBrowserMetricsDirClearedIfReportingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPersistentHistogramsFeature, {{"storage", "MappedFile"}});

  base::FilePath metrics_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &metrics_dir));
  InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  ASSERT_TRUE(base::PathExists(upload_dir));

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Setup the client isn't in sample.
  client->SetHaveMetricsConsent(/* user_consent= */ true,
                                /* app_consent= */ true);
  client->SetInSample(false);
  client->Initialize(prefs.get());
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(base::PathExists(upload_dir));
}

TEST_F(AndroidMetricsServiceClientTest,
       TestBrowserMetricsDirClearedIfNoConsent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPersistentHistogramsFeature, {{"storage", "MappedFile"}});

  base::FilePath metrics_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &metrics_dir));
  InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  ASSERT_TRUE(base::PathExists(upload_dir));

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Setup the client isn't in sample.
  client->SetHaveMetricsConsent(/* user_consent= */ false,
                                /* app_consent= */ false);
  client->Initialize(prefs.get());
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(base::PathExists(upload_dir));
}

TEST_F(AndroidMetricsServiceClientTest,
       TestBrowserMetricsDirExistsIfReportingEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPersistentHistogramsFeature, {{"storage", "MappedFile"}});

  base::FilePath metrics_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &metrics_dir));
  InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  ASSERT_TRUE(base::PathExists(upload_dir));

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Setup the client is in sample.
  client->SetHaveMetricsConsent(/* user_consent= */ true,
                                /* app_consent= */ true);
  client->SetInSample(true);
  client->Initialize(prefs.get());
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(base::PathExists(upload_dir));
}

TEST_F(AndroidMetricsServiceClientTest,
       MetricsServiceCreatedFromInitializeWithNoConsent) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs.get());
  EXPECT_FALSE(client->IsReportingEnabled());
  EXPECT_TRUE(client->GetMetricsService());
}

TEST_F(AndroidMetricsServiceClientTest, GetMetricsServiceIfStarted) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetInSample(true);
  client->Initialize(prefs.get());
  EXPECT_EQ(nullptr, client->GetMetricsServiceIfStarted());
  client->SetHaveMetricsConsent(/* user_consent= */ true,
                                /* app_consent= */ true);
  EXPECT_TRUE(client->GetMetricsServiceIfStarted());
}

TEST_F(AndroidMetricsServiceClientTest,
       ShouldComputeCorrectSampleBucketValues) {
  // The following sample values were generated by using
  // https://www.uuidgenerator.net/version4
  struct {
    const char* client_uuid;
    int expected_sample_bucket_value;
  } test_cases[] = {{"01234567-89ab-40cd-80ef-0123456789ab", 946},
                    {"00aa37bf-7fba-47a7-9180-e334f5c69a8e", 607},
                    {"a7a68d68-8ba3-486d-832b-a0cded65fea2", 995},
                    {"5aed7b5d-b827-400d-9d28-5d23dcc076dc", 802},
                    {"fa5f5bd4-aae7-4d94-ab84-69c8ca40f400", 100}};

  for (const auto& test : test_cases) {
    auto prefs = CreateTestPrefs();
    prefs->SetString(metrics::prefs::kMetricsClientID, test.client_uuid);
    auto client = std::make_unique<SampleBucketValueTestClient>();
    client->SetHaveMetricsConsent(/*user_consent=*/true, /*app_consent=*/true);
    client->Initialize(prefs.get());

    EXPECT_EQ(client->GetSampleBucketValue(),
              test.expected_sample_bucket_value);
  }
}
}  // namespace metrics
