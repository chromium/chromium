// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"

#include <memory>
#include <vector>

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "net/base/ip_address.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class ClientSidePhishingModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      EXPECT_FALSE(model_observer_);
      model_observer_ = observer;
    }
  }

  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      EXPECT_EQ(observer, model_observer_);
      model_observer_ = nullptr;
    }
  }

  // Notifies the model validation observer about the model file update.
  void NotifyModelFileUpdate(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::FilePath& model_file_path,
      const std::vector<base::FilePath>& additional_files_path) {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path)
                                .SetAdditionalFiles(additional_files_path)
                                .Build();
      model_observer_->OnModelUpdated(optimization_target, model_metadata);
    }
  }

 private:
  // The observer that is registered to receive model validation optimzation
  // target events.
  raw_ptr<optimization_guide::OptimizationTargetModelObserver> model_observer_;
};

}  // namespace

class ClientSideDetectionServiceBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    model_observer_tracker_ =
        std::make_unique<ClientSidePhishingModelObserverTracker>();
  }

  void ValidateModel(const base::FilePath& model_file_path,
                     const std::vector<base::FilePath>& additional_file_path) {
    model_observer_tracker_->NotifyModelFileUpdate(
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
        model_file_path, additional_file_path);
    task_environment_.RunUntilIdle();
  }

  void ReadModelAndTfLiteFiles() {
    base::FilePath model_file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
    model_file_path = model_file_path.AppendASCII("components")
                          .AppendASCII("test")
                          .AppendASCII("data")
                          .AppendASCII("safe_browsing")
                          .AppendASCII("client_model.pb");

    base::FilePath additional_files_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                           &additional_files_path);
    additional_files_path = additional_files_path.AppendASCII("components")
                                .AppendASCII("test")
                                .AppendASCII("data")
                                .AppendASCII("safe_browsing");

#if BUILDFLAG(IS_ANDROID)
    additional_files_path =
        additional_files_path.AppendASCII("visual_model_android.tflite");
#else
    additional_files_path =
        additional_files_path.AppendASCII("visual_model_desktop.tflite");
#endif
    ValidateModel(model_file_path, {additional_files_path});
  }

  size_t GetCacheSize() { return service_->cache_.size(); }
  bool HasCacheEntry(const GURL& url) {
    return service_->cache_.find(url) != service_->cache_.end();
  }
  void UpdateCache() { service_->UpdateCache(); }

  bool AddPhishingReport(base::Time timestamp) {
    return service_->AddPhishingReport(timestamp);
  }

  int GetPhishingNumReports() { return service_->GetPhishingNumReports(); }

  bool AtPhishingReportLimit() { return service_->AtPhishingReportLimit(); }

  void AddCacheEntry(const GURL& url, bool is_phishing, base::Time timestamp) {
    service_->AddCacheEntry(url, is_phishing, timestamp);
  }

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    service_->url_loader_factory_ = url_loader_factory;
  }

  void LoadPhishingReportTimesFromPrefs() {
    service_->LoadPhishingReportTimesFromPrefs();
  }

  bool SendClientReportPhishingRequest(
      const GURL& phishing_url,
      float score,
      const std::string& access_token,
      std::optional<ClientSideDetectionType> detection_type = std::nullopt) {
    auto request = std::make_unique<ClientPhishingRequest>();
    request->set_url(phishing_url.spec());
    request->set_client_score(score);
    request->set_is_phishing(true);
    if (detection_type.has_value()) {
      request->set_client_side_detection_type(detection_type.value());
    }

    base::RunLoop run_loop;
    service_->SendClientReportPhishingRequest(
        std::move(request),
        base::BindOnce(&ClientSideDetectionServiceBaseTest::SendRequestDone,
                       base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        access_token);
    phishing_url_ = phishing_url;
    run_loop.Run();
    return is_phishing_;
  }

  void SetResponse(const GURL& url,
                   const std::string& response_data,
                   int net_error) {
    if (net_error != net::OK) {
      test_url_loader_factory_.AddResponse(
          url, network::mojom::URLResponseHead::New(), std::string(),
          network::URLLoaderCompletionStatus(net_error));
      return;
    }
    test_url_loader_factory_.AddResponse(url.spec(), response_data);
  }

  void SetClientReportPhishingResponse(const std::string& response_data,
                                       int net_error) {
    SetResponse(ClientSideDetectionServiceBase::GetClientReportUrl(
                    ClientSideDetectionServiceBase::kClientReportPhishingUrl),
                response_data, net_error);
  }

  std::deque<base::Time>& GetPhishingReportTimes() {
    return service_->phishing_report_times_;
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<ClientSidePhishingModelObserverTracker>
      model_observer_tracker_;
  std::unique_ptr<ClientSideDetectionServiceBase> service_;

 private:
  void SendRequestDone(
      base::OnceClosure continuation_callback,
      GURL phishing_url,
      bool is_phishing,
      std::optional<net::HttpStatusCode> response_code,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict) {
    ASSERT_EQ(phishing_url, phishing_url_);
    is_phishing_ = is_phishing;
    std::move(continuation_callback).Run();
  }

  GURL phishing_url_;
  bool is_phishing_;
};

TEST_F(ClientSideDetectionServiceBaseTest,
       ServiceObjectDeletedBeforeCallbackDone) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();
  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_NE(service_.get(), nullptr);
  // We delete the client-side detection service class even though the callbacks
  // haven't run yet.
  service_.reset();
  // Waiting for the callbacks to run should not crash even if the service
  // object is gone.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ClientSideDetectionServiceBaseTest, AddPhishingReport) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  base::Time now = base::Time::Now();

  EXPECT_TRUE(AddPhishingReport(now));
  EXPECT_EQ(1, GetPhishingNumReports());

  for (int i = 1; i < ClientSideDetectionServiceBase::kMaxReportsPerInterval;
       ++i) {
    EXPECT_TRUE(AddPhishingReport(now));
  }

  EXPECT_FALSE(AddPhishingReport(now));
  EXPECT_EQ(ClientSideDetectionServiceBase::kMaxReportsPerInterval,
            GetPhishingNumReports());
}

TEST_F(ClientSideDetectionServiceBaseTest, LoadPhishingReportTimesFromPrefs) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  base::Time now = base::Time::Now();

  AddPhishingReport(now);
  AddPhishingReport(now - base::Hours(1));
  AddPhishingReport(now - base::Days(2));  // Expired

  // Create a second instance (overwriting service_) to test loading from prefs
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  LoadPhishingReportTimesFromPrefs();
  EXPECT_EQ(2, GetPhishingNumReports());
}

TEST_F(ClientSideDetectionServiceBaseTest, CacheTest) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  GURL url("http://first.url.com/");
  bool is_phishing = false;

  EXPECT_FALSE(service_->GetValidCachedResult(url, &is_phishing));

  base::Time now = base::Time::Now();

  // 1. Positive cache entry (valid: created now)
  AddCacheEntry(GURL("http://first.url.com/"), true, now);

  // 2. Expired negative cache entry (created 1 day + 1 hour ago)
  AddCacheEntry(
      GURL("http://second.url.com/"), false,
      now -
          base::Days(
              ClientSideDetectionServiceBase::kNegativeCacheIntervalDays) -
          base::Hours(1));

  // 3. Expired positive cache entry (created 35 minutes ago)
  AddCacheEntry(
      GURL("http://third.url.com/"), true,
      now -
          base::Minutes(
              ClientSideDetectionServiceBase::kPositiveCacheIntervalMinutes) -
          base::Minutes(5));

  // 4. Valid positive cache entry (created 25 minutes ago)
  AddCacheEntry(
      GURL("http://fourth.url.com/"), true,
      now -
          base::Minutes(
              ClientSideDetectionServiceBase::kPositiveCacheIntervalMinutes) +
          base::Minutes(5));

  // 5. Valid negative cache entry (created 23 hours 55 minutes ago)
  AddCacheEntry(
      GURL("http://fifth.url.com/"), false,
      now -
          base::Days(
              ClientSideDetectionServiceBase::kNegativeCacheIntervalDays) +
          base::Minutes(5));

  // Call UpdateCache
  UpdateCache();

  // The size should be 4 (first, third, fourth, fifth)
  EXPECT_EQ(4U, GetCacheSize());
  EXPECT_TRUE(HasCacheEntry(GURL("http://first.url.com/")));
  EXPECT_FALSE(HasCacheEntry(GURL("http://second.url.com/")));
  EXPECT_TRUE(HasCacheEntry(GURL("http://third.url.com/")));
  EXPECT_TRUE(HasCacheEntry(GURL("http://fourth.url.com/")));
  EXPECT_TRUE(HasCacheEntry(GURL("http://fifth.url.com/")));

  // Retrieve results
  EXPECT_TRUE(service_->GetValidCachedResult(GURL("http://first.url.com/"),
                                             &is_phishing));
  EXPECT_TRUE(is_phishing);

  // Third URL is in cache but is expired (> 30 minutes)
  EXPECT_FALSE(service_->GetValidCachedResult(GURL("http://third.url.com/"),
                                              &is_phishing));

  EXPECT_TRUE(service_->GetValidCachedResult(GURL("http://fourth.url.com/"),
                                             &is_phishing));
  EXPECT_TRUE(is_phishing);

  EXPECT_TRUE(service_->GetValidCachedResult(GURL("http://fifth.url.com/"),
                                             &is_phishing));
  EXPECT_FALSE(is_phishing);
}

TEST_F(ClientSideDetectionServiceBaseTest, IsPrivateIPAddress) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);

  net::IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("10.1.2.3"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("172.24.3.4"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("192.168.1.1"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fc00::"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fec0::"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fec0:1:2::3"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::1"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::ffff:192.168.1.1"));
  EXPECT_TRUE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("1.2.3.4"));
  EXPECT_FALSE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("200.1.1.1"));
  EXPECT_FALSE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("2001:0db8:ac10:fe01::"));
  EXPECT_FALSE(service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::ffff:23c5:281b"));
  EXPECT_FALSE(service_->IsPrivateIPAddress(address));
}

TEST_F(ClientSideDetectionServiceBaseTest, GetNumReportPruningTest) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);

  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::Hours(25);

  // These two should be pruned because they are older than 24 hours
  EXPECT_TRUE(AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(AddPhishingReport(now - twenty_five_hours));

  // These two are within the 24 hour limit
  EXPECT_TRUE(AddPhishingReport(now));
  EXPECT_TRUE(AddPhishingReport(now));

  EXPECT_EQ(2, GetPhishingNumReports());
  EXPECT_FALSE(AtPhishingReportLimit());

  EXPECT_TRUE(AddPhishingReport(now));
  EXPECT_EQ(3, GetPhishingNumReports());
  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_F(ClientSideDetectionServiceBaseTest,
       SendClientReportPhishingRequestWithToken) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token = "fake access token";
  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional("Bearer " + access_token));
        // Cookies should still be included when token is set.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
      }));
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
}

TEST_F(ClientSideDetectionServiceBaseTest,
       SendClientReportPhishingRequestWithoutToken) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token = "";
  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            std::nullopt);
        // Cookies should be attached when token is empty.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
      }));
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
}

TEST_F(ClientSideDetectionServiceBaseTest, TestModelFollowsPrefs) {
  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, false);
  prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, false);
  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());

  // Safe Browsing is not enabled.
  EXPECT_FALSE(service_->IsEnabled());

  // Safe Browsing is enabled.
  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_TRUE(service_->IsEnabled());
}

TEST_F(ClientSideDetectionServiceBaseTest,
       TestReceivingImageEmbedderUpdatesAfterResubscription) {
  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());

  EXPECT_TRUE(service_->IsSubscribedToImageEmbeddingModelUpdates());

  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_FALSE(service_->IsSubscribedToImageEmbeddingModelUpdates());

  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_TRUE(service_->IsSubscribedToImageEmbeddingModelUpdates());
}

class ClientSideDetectionServiceBaseLimitTest
    : public ClientSideDetectionServiceBaseTest,
      public testing::WithParamInterface<bool> {
 public:
  ClientSideDetectionServiceBaseLimitTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (ShouldEnableESBDailyPhishingLimit()) {
      base::FieldTrialParams params;
      params["kMaxReportsPerIntervalESB"] = "10";
      enabled_features.emplace_back(base::test::FeatureRefAndParams{
          kSafeBrowsingDailyPhishingReportsLimit, params});
    } else {
      disabled_features.emplace_back(kSafeBrowsingDailyPhishingReportsLimit);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool ShouldEnableESBDailyPhishingLimit() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ClientSideDetectionServiceBaseLimitTest, AtPhishingReportLimit) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  base::Time now = base::Time::Now();

  EXPECT_FALSE(AtPhishingReportLimit());

  for (int i = 0; i < ClientSideDetectionServiceBase::kMaxReportsPerInterval;
       i++) {
    AddPhishingReport(now);
  }

  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceBaseLimitTest, AtPhishingReportLimitESB) {
  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  base::Time now = base::Time::Now();

  for (int i = 0; i < 3; ++i) {
    AddPhishingReport(now);
  }

  if (ShouldEnableESBDailyPhishingLimit()) {
    // Should NOT be at limit yet for ESB (limit 10)
    EXPECT_FALSE(AtPhishingReportLimit());

    for (int i = 3; i < 10; ++i) {
      AddPhishingReport(now);
    }

    EXPECT_TRUE(AtPhishingReportLimit());
  } else {
    // Standard limit (3) applies.
    EXPECT_TRUE(AtPhishingReportLimit());
  }
}

TEST_P(ClientSideDetectionServiceBaseLimitTest,
       GetNumReportTestWhenPrefsPreloadedAndOverLimit) {
  base::ListValue time_list;
  for (int i = 0; i < ClientSideDetectionServiceBase::kMaxReportsPerInterval;
       ++i) {
    time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));
  }

  prefs_.SetList(prefs::kSafeBrowsingCsdPingTimestamps, std::move(time_list));

  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());
  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceBaseLimitTest,
       GetNumReportTestWhenPrefsPreloadedNotOverLimit) {
  base::ListValue time_list;
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));

  prefs_.SetList(prefs::kSafeBrowsingCsdPingTimestamps, std::move(time_list));

  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());
  EXPECT_FALSE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceBaseLimitTest, GetNumReportTestESB) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();

  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::Hours(25);
  EXPECT_TRUE(AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(AddPhishingReport(now));
  EXPECT_TRUE(AddPhishingReport(now));

  EXPECT_EQ(2, GetPhishingNumReports());
  // We have not quite hit the limit for both ESB and SSB users.
  EXPECT_FALSE(AtPhishingReportLimit());

  // Adding one more will hit the limit just for SSB users.
  EXPECT_TRUE(AddPhishingReport(now));

  EXPECT_EQ(3, GetPhishingNumReports());
  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit)) {
    EXPECT_FALSE(AtPhishingReportLimit());
  } else {
    EXPECT_TRUE(AtPhishingReportLimit());
  }

  // Adding 7 more to 10 reports total will hit the limit for ESB users as the
  // limit is predefined in this class.

  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit)) {
    EXPECT_TRUE(AddPhishingReport(now));
    EXPECT_TRUE(AddPhishingReport(now));
    EXPECT_TRUE(AddPhishingReport(now));
    EXPECT_TRUE(AddPhishingReport(now));
    EXPECT_TRUE(AddPhishingReport(now));
    EXPECT_TRUE(AddPhishingReport(now));
    EXPECT_TRUE(AddPhishingReport(now));
  } else {
    EXPECT_FALSE(AddPhishingReport(now));
    EXPECT_FALSE(AddPhishingReport(now));
    EXPECT_FALSE(AddPhishingReport(now));
    EXPECT_FALSE(AddPhishingReport(now));
    EXPECT_FALSE(AddPhishingReport(now));
    EXPECT_FALSE(AddPhishingReport(now));
    EXPECT_FALSE(AddPhishingReport(now));
  }

  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceBaseLimitTest,
       SendClientReportPhishingRequest) {
  service_ = ClientSideDetectionServiceBase::CreateForTesting(&prefs_, nullptr);
  SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token;

  // Safe browsing is not enabled.
  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, false);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score, access_token));

  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);
  base::Time before = base::Time::Now();

  // Invalid response body from the server, but we will still track it as a
  // ping count.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  SetClientReportPhishingResponse("invalid proto response", net::OK);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score, access_token));
  histogram_tester->ExpectUniqueSample(
      /*name=*/"SBClientPhishing.NetworkResult2",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);

  // Triggering user report should not contribute to ping count.
  ClientPhishingResponse response;
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(
      url, score, access_token, ClientSideDetectionType::USER_REPORT));
  EXPECT_FALSE(AtPhishingReportLimit());

  // Normal behavior with no access token.
  histogram_tester = std::make_unique<base::HistogramTester>();
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
  histogram_tester->ExpectUniqueSample(
      /*name=*/"SBClientPhishing.NetworkResult2",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);

  // This request will fail, but not because of the cap, but because the network
  // failed, but we will still log the number of pings sent.
  histogram_tester = std::make_unique<base::HistogramTester>();
  EXPECT_FALSE(AtPhishingReportLimit());
  GURL second_url("http://b.com/");
  response.set_phishy(false);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  net::ERR_FAILED);
  EXPECT_FALSE(
      SendClientReportPhishingRequest(second_url, score, access_token));
  histogram_tester->ExpectUniqueSample(
      /*name=*/"SBClientPhishing.NetworkResult2",
      /*sample=*/net::ERR_FAILED,
      /*expected_bucket_count=*/1);

  // We have sent 3 pings so far, which is the cap.
  EXPECT_TRUE(AtPhishingReportLimit());

  // Even if we are at the limit, user report should still be triggered.
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(
      url, score, access_token, ClientSideDetectionType::USER_REPORT));
  EXPECT_TRUE(AtPhishingReportLimit());

  // Although this is a normal behavior, we are capped in the number of pings,
  // so this will expect false.
  GURL third_url("http://c.com/");
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_FALSE(SendClientReportPhishingRequest(third_url, score, access_token));

  base::Time after = base::Time::Now();

  // Check that we have recorded 3 requests within the correct time range. The
  // third_url is not recorded because the send was attempted while we are at
  // the limit.
  std::deque<base::Time>& report_times = GetPhishingReportTimes();
  EXPECT_EQ(3U, report_times.size());
  EXPECT_TRUE(AtPhishingReportLimit());
  while (!report_times.empty()) {
    base::Time time = report_times.back();
    report_times.pop_back();
    EXPECT_LE(before, time);
    EXPECT_GE(after, time);
  }

  // Only the first url should be in the cache.
  bool is_phishing;
  EXPECT_TRUE(service_->GetValidCachedResult(url, &is_phishing));
  EXPECT_TRUE(is_phishing);
  bool is_second_url_phishing = false;
  EXPECT_FALSE(
      service_->GetValidCachedResult(second_url, &is_second_url_phishing));
  EXPECT_FALSE(is_second_url_phishing);
  bool is_third_url_phishing = false;
  EXPECT_FALSE(
      service_->GetValidCachedResult(third_url, &is_third_url_phishing));
  EXPECT_FALSE(is_third_url_phishing);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSideDetectionServiceBaseLimitTest,
                         testing::Bool());

class ClientSideDetectionServiceBaseOnlyESBTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ClientSideDetectionServiceBaseOnlyESBTest() {}

  bool is_esb_enabled() const { return std::get<0>(GetParam()); }
  bool is_feature_enabled() const { return std::get<1>(GetParam()); }

 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());
    if (is_feature_enabled()) {
      feature_list_.InitAndEnableFeature(
          kClientSideDetectionOnlyESBClassification);
    } else {
      feature_list_.InitAndDisableFeature(
          kClientSideDetectionOnlyESBClassification);
    }
    model_observer_tracker_ =
        std::make_unique<ClientSidePhishingModelObserverTracker>();
  }

  void TearDown() override { service_.reset(); }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ClientSidePhishingModelObserverTracker>
      model_observer_tracker_;
  std::unique_ptr<ClientSideDetectionServiceBase> service_;
};

TEST_P(ClientSideDetectionServiceBaseOnlyESBTest,
       TestReceivingImageClassifierUpdatesAfterResubscription) {
  prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, is_esb_enabled());

  service_ = ClientSideDetectionServiceBase::CreateForTesting(
      &prefs_, model_observer_tracker_.get());

  if (is_feature_enabled()) {
    EXPECT_EQ(service_->IsSubscribedToImageClassifierModelUpdates(),
              is_esb_enabled());
  } else {
    EXPECT_TRUE(service_->IsSubscribedToImageClassifierModelUpdates());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSideDetectionServiceBaseOnlyESBTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace safe_browsing
