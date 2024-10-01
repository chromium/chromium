// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/remote_database_manager.h"

#include <map>
#include <memory>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
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
    DCHECK(base::Contains(urls_safebrowsing_threat_type_, url));
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
      : db_(db),
        expected_url_(expected_url),
        expected_threat_type_(expected_threat_type) {}

  ~TestClient() override { db_->CancelCheck(this); }

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override {
    EXPECT_EQ(expected_url_, url);
    EXPECT_EQ(expected_threat_type_, threat_type);
    is_callback_called_ = true;
  }

  bool IsCallbackCalled() { return is_callback_called_; }

 private:
  scoped_refptr<RemoteSafeBrowsingDatabaseManager> db_;
  GURL expected_url_;
  SBThreatType expected_threat_type_;
  bool is_callback_called_ = false;
};

}  // namespace

class RemoteDatabaseManagerTest : public testing::Test {
 protected:
  using enum SBThreatType;

  RemoteDatabaseManagerTest() {}

  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    db_ = new RemoteSafeBrowsingDatabaseManager();
    db_->StartOnUIThread(test_shared_loader_factory_,
                         GetTestV4ProtocolConfig());

    url_interceptor_ = std::make_unique<TestUrlCheckInterceptor>();
    SafeBrowsingApiHandlerBridge::GetInstance().SetInterceptorForTesting(
        url_interceptor_.get());
  }

  void TearDown() override {
    db_->StopOnUIThread(/*shutdown=*/false);
    db_ = nullptr;
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

TEST_F(RemoteDatabaseManagerTest, ThreatSource) {
  EXPECT_EQ(ThreatSource::ANDROID_SAFEBROWSING,
            db_->GetBrowseUrlThreatSource(CheckBrowseUrlType::kHashDatabase));
  EXPECT_EQ(ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME,
            db_->GetBrowseUrlThreatSource(CheckBrowseUrlType::kHashRealTime));
}

}  // namespace safe_browsing
