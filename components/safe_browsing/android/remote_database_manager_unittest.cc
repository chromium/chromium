// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/remote_database_manager.h"

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_test_util.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/sb_test_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

// Used to override response from SafeBrowsingApiHandlerBridge.
class TestUrlCheckInterceptor : public safe_browsing::UrlCheckInterceptor {
 public:
  TestUrlCheckInterceptor() = default;
  ~TestUrlCheckInterceptor() override = default;

  // Checks the threat type of |url| previously set by
  // |SetSafeBrowsingThreatTypeForUrl|. It crashes if the threat type of |url|
  // is not set in advance.
  void CheckBySafeBrowsing(
      SafeBrowsingApiHandlerBridge::ResponseCallback callback,
      const GURL& gurl) override {
    std::string url = gurl.spec();
    DCHECK(urls_safebrowsing_threat_type_.contains(url));
    std::move(callback).Run(urls_safebrowsing_threat_type_[url],
                            ThreatMetadata());
  }

  void SetSafeBrowsingThreatTypeForUrl(const GURL& url,
                                       SBThreatType threat_type) {
    urls_safebrowsing_threat_type_[url.spec()] = threat_type;
  }

 private:
  base::flat_map<std::string, SBThreatType> urls_safebrowsing_threat_type_;
};

// Used to verify the result returned from RemoteDatabaseManager is expected.
class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient(scoped_refptr<RemoteSafeBrowsingDatabaseManager> db,
             const GURL& expected_url,
             SBThreatType expected_threat_type)
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        db_(db),
        expected_url_(expected_url),
        expected_threat_type_(expected_threat_type) {}

  ~TestClient() override { db_->CancelCheck(this); }

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type) override {
    OnResult(url, threat_type);
  }

  void OnCheckSubresourceFilterUrlResult(
      const GURL& url,
      SBThreatType threat_type,
      const SubresourceFilterMatch& subresource_filter_match) override {
    OnResult(url, threat_type);
  }

  bool IsCallbackCalled() { return is_callback_called_; }

  void set_on_callback_done(base::OnceClosure on_callback_done) {
    on_callback_done_ = std::move(on_callback_done);
  }

  void set_custom_callback_task_runner(
      scoped_refptr<base::SequencedTaskRunner> custom_callback_task_runner) {
    custom_callback_task_runner_ = std::move(custom_callback_task_runner);
  }

  scoped_refptr<base::SequencedTaskRunner> GetCustomCallbackTaskRunner()
      override {
    return custom_callback_task_runner_;
  }

  base::WeakPtr<V5GetHashProtocolManager> GetV5GetHashProtocolManager()
      override {
    NOTREACHED();
  }

 private:
  void OnResult(const GURL& url, SBThreatType threat_type) {
    EXPECT_EQ(expected_url_, url);
    EXPECT_EQ(expected_threat_type_, threat_type);
    is_callback_called_ = true;
    if (on_callback_done_) {
      std::move(on_callback_done_).Run();
    }
  }

  scoped_refptr<RemoteSafeBrowsingDatabaseManager> db_;
  GURL expected_url_;
  SBThreatType expected_threat_type_;
  bool is_callback_called_ = false;
  base::OnceClosure on_callback_done_;
  scoped_refptr<base::SequencedTaskRunner> custom_callback_task_runner_;
};

}  // namespace

class RemoteDatabaseManagerTest
    : public testing::Test,
      public safe_browsing::test::WithMockSafeBrowsingApiHandler {
 protected:
  using enum SBThreatType;

  RemoteDatabaseManagerTest() = default;

  void SetUp() override {
    safe_browsing::test::WithMockSafeBrowsingApiHandler::SetUp();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    db_ = base::MakeRefCounted<RemoteSafeBrowsingDatabaseManager>();
    db_->StartOnUIThread(test_shared_loader_factory_,
                         GetTestSBProtocolConfig());

    url_interceptor_ = std::make_unique<TestUrlCheckInterceptor>();
    SafeBrowsingApiHandlerBridge::GetInstance().SetInterceptorForTesting(
        url_interceptor_.get());
  }

  void TearDown() override {
    db_->StopOnUIThread(/*shutdown=*/false);
    db_ = nullptr;
    safe_browsing::test::WithMockSafeBrowsingApiHandler::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestUrlCheckInterceptor> url_interceptor_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  scoped_refptr<RemoteSafeBrowsingDatabaseManager> db_;
  base::HistogramTester histogram_tester_;
};

TEST_F(RemoteDatabaseManagerTest, CheckBrowseUrl_HashDatabase) {
  GURL url("https://example.com");
  url_interceptor_->SetSafeBrowsingThreatTypeForUrl(
      url, SB_THREAT_TYPE_URL_PHISHING);
  TestClient client(db_, /*expected_url=*/url,
                    /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING);

  db_->CheckBrowseUrl(url, {SB_THREAT_TYPE_URL_PHISHING}, &client,
                      CheckBrowseUrlType::kHashDatabase);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client.IsCallbackCalled());
  histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CanCheckUrl",
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "SB2.RemoteCall.CanCheckUrl.HashDatabase",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount("SB2.RemoteCall.CanCheckUrl.HashRealTime",
                                     /*expected_count=*/0);
}

TEST_F(RemoteDatabaseManagerTest, CheckBrowseUrl_HashRealtime) {
  GURL url("https://example.com");
  url_interceptor_->SetSafeBrowsingThreatTypeForUrl(
      url, SB_THREAT_TYPE_URL_PHISHING);
  TestClient client(db_, /*expected_url=*/url,
                    /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING);

  db_->CheckBrowseUrl(url, {SB_THREAT_TYPE_URL_PHISHING}, &client,
                      CheckBrowseUrlType::kHashRealTime);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client.IsCallbackCalled());
  histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CanCheckUrl",
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "SB2.RemoteCall.CanCheckUrl.HashRealTime",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount("SB2.RemoteCall.CanCheckUrl.HashDatabase",
                                     /*expected_count=*/0);
}

TEST_F(RemoteDatabaseManagerTest, CheckBrowseUrl_CustomCallbackTaskRunner) {
  SafeBrowsingApiHandlerBridge::GetInstance().SetInterceptorForTesting(nullptr);
  auto custom_callback_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();

  // `kHashDatabase` posts completion onto `custom_callback_runner`.
  GURL hash_db_url("https://hashdb.example.com");
  AddSafeBrowsingResponse(
      hash_db_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING},
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);
  TestClient hash_db_client(
      db_, /*expected_url=*/hash_db_url,
      /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING);
  hash_db_client.set_custom_callback_task_runner(custom_callback_runner);

  EXPECT_FALSE(db_->CheckBrowseUrl(hash_db_url, {SB_THREAT_TYPE_URL_PHISHING},
                                   &hash_db_client,
                                   CheckBrowseUrlType::kHashDatabase));
  EXPECT_FALSE(hash_db_client.IsCallbackCalled());
  EXPECT_TRUE(custom_callback_runner->HasPendingTask());
  custom_callback_runner->RunPendingTasks();
  EXPECT_TRUE(hash_db_client.IsCallbackCalled());

  // `kHashRealTime` stays on the default UI thread task runner.
  GURL hash_rt_url("https://hashrt.example.com");
  AddSafeBrowsingResponse(
      hash_rt_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME,
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING},
      SafeBrowsingJavaProtocol::REAL_TIME);
  base::RunLoop hash_rt_run_loop;
  TestClient hash_rt_client(
      db_, /*expected_url=*/hash_rt_url,
      /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING);
  hash_rt_client.set_custom_callback_task_runner(custom_callback_runner);
  hash_rt_client.set_on_callback_done(hash_rt_run_loop.QuitClosure());

  EXPECT_FALSE(db_->CheckBrowseUrl(hash_rt_url, {SB_THREAT_TYPE_URL_PHISHING},
                                   &hash_rt_client,
                                   CheckBrowseUrlType::kHashRealTime));
  EXPECT_FALSE(custom_callback_runner->HasPendingTask());
  hash_rt_run_loop.Run();
  EXPECT_TRUE(hash_rt_client.IsCallbackCalled());

  // `CheckUrlForSubresourceFilter` stays on the default UI thread task runner.
  GURL subresource_url("https://subresource.example.com");
  AddSafeBrowsingResponse(
      subresource_url, SafeBrowsingApiLookupResult::SUCCESS,
      SafeBrowsingJavaThreatType::NO_THREAT, {},
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST,
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING,
       SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION},
      SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);
  base::RunLoop subresource_run_loop;
  TestClient subresource_client(db_, /*expected_url=*/subresource_url,
                                /*expected_threat_type=*/SB_THREAT_TYPE_SAFE);
  subresource_client.set_custom_callback_task_runner(custom_callback_runner);
  subresource_client.set_on_callback_done(subresource_run_loop.QuitClosure());

  EXPECT_FALSE(
      db_->CheckUrlForSubresourceFilter(subresource_url, &subresource_client));
  EXPECT_FALSE(custom_callback_runner->HasPendingTask());
  subresource_run_loop.Run();
  EXPECT_TRUE(subresource_client.IsCallbackCalled());
}

TEST_F(RemoteDatabaseManagerTest, ThreatSource) {
  EXPECT_EQ(ThreatSource::ANDROID_SAFEBROWSING,
            db_->GetBrowseUrlThreatSource(CheckBrowseUrlType::kHashDatabase));
  EXPECT_EQ(ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME,
            db_->GetBrowseUrlThreatSource(CheckBrowseUrlType::kHashRealTime));
}

TEST_F(RemoteDatabaseManagerTest, MatchDownloadAllowlistUrl) {
  GURL allowlisted_url{"https://www.example.test"};
  AddLocalAllowlistEntry(allowlisted_url, /*is_download_allowlist=*/true,
                         /*is_match=*/true);

  // Allowlisted URL should match.
  {
    base::test::TestFuture<bool> result_future;
    db_->MatchDownloadAllowlistUrl(allowlisted_url,
                                   result_future.GetCallback());
    EXPECT_TRUE(result_future.Get());
  }

  // Not on the allowlist.
  {
    GURL url{"https://www.notexample.test"};
    base::test::TestFuture<bool> result_future;
    db_->MatchDownloadAllowlistUrl(url, result_future.GetCallback());
    EXPECT_FALSE(result_future.Get());
  }

  // Not checked because of URL scheme.
  {
    GURL url{"data:,Hello%2C%20World%21"};
    base::test::TestFuture<bool> result_future;
    db_->MatchDownloadAllowlistUrl(url, result_future.GetCallback());
    EXPECT_FALSE(result_future.Get());
  }
  {
    GURL url{"file:///usr/home/foo"};
    base::test::TestFuture<bool> result_future;
    db_->MatchDownloadAllowlistUrl(url, result_future.GetCallback());
    EXPECT_FALSE(result_future.Get());
  }
}

}  // namespace safe_browsing
