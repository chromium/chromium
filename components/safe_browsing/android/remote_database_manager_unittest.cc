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
#include "base/time/time.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
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
  // |SetSafetyNetThreatTypeForUrl|. It crashes if the threat type of |url| is
  // not set in advance.
  void CheckBySafetyNet(
      std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback,
      const GURL& gurl) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_safetynet_threat_type_, url));
    std::move(*callback).Run(urls_safetynet_threat_type_[url],
                             ThreatMetadata());
  }

  // Checks the threat type of |url| previously set by
  // |SetSafeBrowsingThreatTypeForUrl|. It crashes if the threat type of |url|
  // is not set in advance.
  void CheckBySafeBrowsing(
      std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback,
      const GURL& gurl) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_safebrowsing_threat_type_, url));
    std::move(*callback).Run(urls_safebrowsing_threat_type_[url],
                             ThreatMetadata());
  }

  void SetSafetyNetThreatTypeForUrl(const GURL& url, SBThreatType threat_type) {
    urls_safetynet_threat_type_[url.spec()] = threat_type;
  }

  void SetSafeBrowsingThreatTypeForUrl(const GURL& url,
                                       SBThreatType threat_type) {
    urls_safebrowsing_threat_type_[url.spec()] = threat_type;
  }

 private:
  base::flat_map<std::string, SBThreatType> urls_safetynet_threat_type_;
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
  RemoteDatabaseManagerTest() {}

  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    db_ = new RemoteSafeBrowsingDatabaseManager();
    db_->StartOnSBThread(test_shared_loader_factory_,
                         GetTestV4ProtocolConfig());

    url_interceptor_ = std::make_unique<TestUrlCheckInterceptor>();
    SafeBrowsingApiHandlerBridge::GetInstance().SetInterceptorForTesting(
        url_interceptor_.get());
  }

  void TearDown() override {
    db_->StopOnSBThread(/*shutdown=*/false);
    db_ = nullptr;
  }

  // Setup the two field trial params.  These are read in db_'s ctor.
  void SetFieldTrialParams(const std::string types_to_check_val) {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();

    const std::string group_name = "GroupFoo";  // Value not used
    const std::string experiment_name = "SafeBrowsingAndroid";
    ASSERT_TRUE(
        base::FieldTrialList::CreateFieldTrial(experiment_name, group_name));

    std::map<std::string, std::string> params;
    if (!types_to_check_val.empty())
      params["types_to_check"] = types_to_check_val;

    ASSERT_TRUE(
        base::AssociateFieldTrialParams(experiment_name, group_name, params));
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
  url_interceptor_->SetSafetyNetThreatTypeForUrl(url,
                                                 SB_THREAT_TYPE_URL_PHISHING);
  TestClient client(db_, /*expected_url=*/url,
                    /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING);

  db_->CheckBrowseUrl(url, {SB_THREAT_TYPE_URL_PHISHING}, &client,
                      MechanismExperimentHashDatabaseCache::kNoExperiment,
                      CheckBrowseUrlType::kHashDatabase);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client.IsCallbackCalled());
  histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CanCheckUrl",
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
}

TEST_F(RemoteDatabaseManagerTest, CheckBrowseUrl_HashRealtime) {
  GURL url("https://example.com");
  url_interceptor_->SetSafeBrowsingThreatTypeForUrl(
      url, SB_THREAT_TYPE_URL_PHISHING);
  TestClient client(db_, /*expected_url=*/url,
                    /*expected_threat_type=*/SB_THREAT_TYPE_URL_PHISHING);

  db_->CheckBrowseUrl(url, {SB_THREAT_TYPE_URL_PHISHING}, &client,
                      MechanismExperimentHashDatabaseCache::kNoExperiment,
                      CheckBrowseUrlType::kHashRealTime);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client.IsCallbackCalled());
  histogram_tester_.ExpectUniqueSample("SB2.RemoteCall.CanCheckUrl",
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
}

TEST_F(RemoteDatabaseManagerTest, DestinationsToCheckDefault) {
  // Most are true, a few are false.
  for (int t_int = 0;
       t_int <= static_cast<int>(network::mojom::RequestDestination::kMaxValue);
       t_int++) {
    network::mojom::RequestDestination t =
        static_cast<network::mojom::RequestDestination>(t_int);
    switch (t) {
      case network::mojom::RequestDestination::kStyle:
      case network::mojom::RequestDestination::kImage:
      case network::mojom::RequestDestination::kFont:
        EXPECT_FALSE(db_->CanCheckRequestDestination(t));
        break;
      default:
        EXPECT_TRUE(db_->CanCheckRequestDestination(t));
        break;
    }
  }
}

TEST_F(RemoteDatabaseManagerTest, DestinationsToCheckFromTrial) {
  SetFieldTrialParams("7,16,blah, 20");
  // Stop the current DB and start a new one to consume the new field trial
  // params.
  db_->StopOnSBThread(/*shutdown=*/false);
  db_ = new RemoteSafeBrowsingDatabaseManager();
  db_->StartOnSBThread(test_shared_loader_factory_, GetTestV4ProtocolConfig());
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kDocument));  // defaulted
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kIframe));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kFrame));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kFencedframe));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kStyle));
  EXPECT_FALSE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kScript));
  EXPECT_FALSE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kImage));
  // ...
  EXPECT_FALSE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kVideo));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kWorker));
}

}  // namespace safe_browsing
