// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/database_manager.h"

#include <stddef.h>

#include <set>
#include <string>

#include "base/base64.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "crypto/hash.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient()
      : SafeBrowsingDatabaseManager::Client(GetPassKeyForTesting()),
        callback_invoked_(false),
        is_notification_abusive_(false) {}

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  ~TestClient() override = default;

  void OnCheckNotificationAbuseUrlResult(bool is_abusive) override {
    is_notification_abusive_ = is_abusive;
    callback_invoked_ = true;
    run_loop_.Quit();
  }

  bool is_notification_abusive() const { return is_notification_abusive_; }

  void WaitForCallback() { run_loop_.Run(); }

  bool callback_invoked() { return callback_invoked_; }

  base::WeakPtr<V5GetHashProtocolManager> GetV5GetHashProtocolManager()
      override {
    return v5_get_hash_protocol_manager_;
  }

  void SetV5GetHashProtocolManager(
      base::WeakPtr<V5GetHashProtocolManager> manager) {
    v5_get_hash_protocol_manager_ = manager;
  }

 private:
  base::WeakPtr<V5GetHashProtocolManager> v5_get_hash_protocol_manager_;
  bool callback_invoked_;
  bool is_notification_abusive_;
  base::RunLoop run_loop_;
};

}  // namespace

class SafeBrowsingDatabaseManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    db_manager_ = new TestSafeBrowsingDatabaseManager(
        base::SequencedTaskRunner::GetCurrentDefault());
    db_manager_->StartOnUIThread(test_shared_loader_factory_,
                                 GetTestV4ProtocolConfig());
  }

  void TearDown() override {
    db_manager_->StopOnUIThread(/*shutdown=*/false);
    db_manager_ = nullptr;
  }

  std::string GetV4GetHashResponseWithPermissions(
      const std::vector<std::string>& permissions) {
    ListIdentifier list_id = GetChromeUrlApiId();
    FullHashStr full_hash =
        std::string(base::as_string_view(crypto::hash::Sha256("example.com/")));

    FindFullHashesResponse response;
    response.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* m = response.add_matches();
    m->set_platform_type(list_id.platform_type());
    m->set_threat_entry_type(list_id.threat_entry_type());
    m->set_threat_type(list_id.threat_type());
    m->mutable_threat()->set_hash(full_hash);
    m->mutable_cache_duration()->set_seconds(300);

    for (const std::string& permission : permissions) {
      ThreatEntryMetadata::MetadataEntry* e =
          m->mutable_threat_entry_metadata()->add_entries();
      e->set_key("permission");
      e->set_value(permission);
    }

    std::string res_data;
    response.SerializeToString(&res_data);
    return res_data;
  }

  std::string GetStockV4GetHashResponse() {
    return GetV4GetHashResponseWithPermissions({"NOTIFICATIONS"});
  }

  void SetUpV5Client(TestClient& client) {
    v5_cache_ =
        std::make_unique<V5SearchHashesCache>(/*history_service=*/nullptr);
    v5_manager_ = std::make_unique<V5GetHashProtocolManager>(
        test_shared_loader_factory_, GetTestV4ProtocolConfig(),
        v5_cache_.get());
    client.SetV5GetHashProtocolManager(v5_manager_->GetWeakPtr());
  }

  void AddNotificationAbuseResponse(const std::string& request_url_spec,
                                    bool is_abusive,
                                    V5::ThreatType v5_threat_type) {
    if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
      V5::SearchHashesResponse response;
      response.mutable_cache_duration()->set_seconds(300);
      if (is_abusive) {
        V5::FullHash* full_hash = response.add_full_hashes();
        full_hash->set_full_hash(std::string(
            base::as_string_view(crypto::hash::Sha256("example.com/"))));
        V5::FullHash::FullHashDetail* detail =
            full_hash->add_full_hash_details();
        detail->set_threat_type(v5_threat_type);
      }
      std::string response_data;
      response.SerializeToString(&response_data);
      test_url_loader_factory_.AddResponse(request_url_spec, response_data);
    } else {
      if (is_abusive) {
        test_url_loader_factory_.AddResponse(request_url_spec,
                                             GetStockV4GetHashResponse());
      } else {
        std::vector<std::string> permissions = {"", "Stuff", "NOTIFICATION",
                                                "notifications", "GEOLOCATION"};
        test_url_loader_factory_.AddResponse(
            request_url_spec, GetV4GetHashResponseWithPermissions(permissions));
      }
    }
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestSafeBrowsingDatabaseManager> db_manager_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<V5SearchHashesCache> v5_cache_;
  std::unique_ptr<V5GetHashProtocolManager> v5_manager_;
};

// Test fixture parameterized over whether Safe Browsing v5 is enabled.
class SafeBrowsingDatabaseManagerTest_V4V5
    : public SafeBrowsingDatabaseManagerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SafeBrowsingDatabaseManagerTest_V4V5() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
    } else {
      feature_list_.InitAndDisableFeature(kLocalListsUseSBv5);
    }
  }

  bool IsV5() const { return GetParam(); }

  void SetUpV5ClientIfNeeded(TestClient& client) {
    if (IsV5()) {
      SetUpV5Client(client);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SafeBrowsingDatabaseManagerTest_V4V5,
       CheckNotificationAbuseUrlWrongScheme) {
  TestClient client;
  SetUpV5ClientIfNeeded(client);
  EXPECT_TRUE(db_manager_->CheckNotificationAbuseUrl(GURL("file://example.txt"),
                                                     &client));
}

TEST_P(SafeBrowsingDatabaseManagerTest_V4V5, CancelNotificationAbuseCheck) {
  TestClient client;
  SetUpV5ClientIfNeeded(client);
  const GURL url("https://www.example.com/more");

  EXPECT_FALSE(db_manager_->CheckNotificationAbuseUrl(url, &client));
  EXPECT_TRUE(db_manager_->CancelNotificationAbuseCheck(&client));

  EXPECT_FALSE(client.callback_invoked());
  EXPECT_FALSE(client.is_notification_abusive());
}

TEST_P(SafeBrowsingDatabaseManagerTest_V4V5,
       GetNotificationAbuseCheckResponse_Abusive) {
  TestClient client;
  SetUpV5ClientIfNeeded(client);
  const GURL url("https://www.example.com/more");

  GURL request_url;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_url = request.url;
      }));

  EXPECT_FALSE(db_manager_->CheckNotificationAbuseUrl(url, &client));
  AddNotificationAbuseResponse(
      request_url.spec(), /*is_abusive=*/true,
      /*v5_threat_type=*/V5::ThreatType::NOTIFICATION_ABUSE);

  client.WaitForCallback();
  EXPECT_TRUE(client.is_notification_abusive());
}

TEST_P(SafeBrowsingDatabaseManagerTest_V4V5,
       GetNotificationAbuseCheckResponse_NotAbusive) {
  TestClient client;
  SetUpV5ClientIfNeeded(client);
  const GURL url("https://www.example.com/more");

  GURL request_url;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_url = request.url;
      }));

  EXPECT_FALSE(db_manager_->CheckNotificationAbuseUrl(url, &client));
  AddNotificationAbuseResponse(
      request_url.spec(), /*is_abusive=*/false,
      /*v5_threat_type=*/V5::ThreatType::NOTIFICATION_ABUSE);

  client.WaitForCallback();
  EXPECT_FALSE(client.is_notification_abusive());
}

// Test fixture with Safe Browsing v5 enabled.
class SafeBrowsingDatabaseManagerTest_V5
    : public SafeBrowsingDatabaseManagerTest {
 public:
  SafeBrowsingDatabaseManagerTest_V5() {
    feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SafeBrowsingDatabaseManagerTest_V5,
       GetNotificationAbuseCheckResponse_NonApiAbuseThreatTypeIgnored) {
  TestClient client;
  SetUpV5Client(client);
  const GURL url("https://www.example.com/more");

  GURL request_url;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_url = request.url;
      }));

  EXPECT_FALSE(db_manager_->CheckNotificationAbuseUrl(url, &client));
  AddNotificationAbuseResponse(request_url.spec(), /*is_abusive=*/true,
                               /*v5_threat_type=*/V5::ThreatType::MALWARE);

  client.WaitForCallback();
  EXPECT_FALSE(client.is_notification_abusive());
}

TEST_F(SafeBrowsingDatabaseManagerTest_V5,
       GetNotificationAbuseCheckResponse_V5_NullManagerReturnsTrue) {
  TestClient client;
  const GURL url("https://www.example.com/more");

  EXPECT_TRUE(db_manager_->CheckNotificationAbuseUrl(url, &client));
  EXPECT_FALSE(client.callback_invoked());
}

TEST_F(SafeBrowsingDatabaseManagerTest_V5,
       GetNotificationAbuseCheckResponse_V5_ManagerDestroyedReturnsSafe) {
  TestClient client;
  SetUpV5Client(client);
  const GURL url("https://www.example.com/more");

  GURL request_url;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_url = request.url;
      }));

  EXPECT_FALSE(db_manager_->CheckNotificationAbuseUrl(url, &client));

  // Stage an abusive response (which should not be used).
  AddNotificationAbuseResponse(
      request_url.spec(), /*is_abusive=*/true,
      /*v5_threat_type=*/V5::ThreatType::NOTIFICATION_ABUSE);
  EXPECT_FALSE(client.callback_invoked());

  // Destroy the V5GetHashProtocolManager while the check is in-flight.
  v5_manager_.reset();

  client.WaitForCallback();
  EXPECT_TRUE(client.callback_invoked());
  EXPECT_FALSE(client.is_notification_abusive());

  // Confirm that the check was removed from internal tracking by issuing
  // another check for the same client. Since v5_manager_ is now null, it should
  // return true synchronously without failing duplicate check CHECKs.
  EXPECT_TRUE(db_manager_->CheckNotificationAbuseUrl(url, &client));
}

TEST_P(SafeBrowsingDatabaseManagerTest_V4V5,
       StopOnUIThreadCancelsPendingNotificationAbuseCheck) {
  TestClient client;
  SetUpV5ClientIfNeeded(client);
  const GURL url("https://www.example.com/more");

  GURL request_url;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_url = request.url;
      }));

  EXPECT_FALSE(db_manager_->CheckNotificationAbuseUrl(url, &client));

  // Stage an abusive response (which should not be used).
  AddNotificationAbuseResponse(
      request_url.spec(), /*is_abusive=*/true,
      /*v5_threat_type=*/V5::ThreatType::NOTIFICATION_ABUSE);
  EXPECT_FALSE(client.callback_invoked());

  db_manager_->StopOnUIThread(/*shutdown=*/false);

  EXPECT_TRUE(client.callback_invoked());
  EXPECT_FALSE(client.is_notification_abusive());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SafeBrowsingDatabaseManagerTest_V4V5,
                         testing::Bool());

}  // namespace safe_browsing
