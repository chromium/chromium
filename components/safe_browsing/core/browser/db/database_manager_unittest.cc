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
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient() : callback_invoked_(false) {}

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  ~TestClient() override {}

  void OnCheckApiBlocklistUrlResult(const GURL& url,
                                    const ThreatMetadata& metadata) override {
    blocked_permissions_ = metadata.api_permissions;
    callback_invoked_ = true;
    run_loop_.Quit();
  }

  const std::set<std::string>& GetBlockedPermissions() {
    return blocked_permissions_;
  }

  void WaitForCallback() { run_loop_.Run(); }

  bool callback_invoked() { return callback_invoked_; }

 private:
  std::set<std::string> blocked_permissions_;
  bool callback_invoked_;
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
    db_manager_->StopOnUIThread(false);
    db_manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  std::string GetStockV4GetHashResponse() {
    ListIdentifier list_id = GetChromeUrlApiId();
    FullHashStr full_hash = crypto::SHA256HashString("example.com/");

    FindFullHashesResponse response;
    response.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* m = response.add_matches();
    m->set_platform_type(list_id.platform_type());
    m->set_threat_entry_type(list_id.threat_entry_type());
    m->set_threat_type(list_id.threat_type());
    m->mutable_threat()->set_hash(full_hash);
    m->mutable_cache_duration()->set_seconds(300);

    ThreatEntryMetadata::MetadataEntry* e =
        m->mutable_threat_entry_metadata()->add_entries();
    e->set_key("permission");
    e->set_value("GEOLOCATION");

    std::string res_data;
    response.SerializeToString(&res_data);
    return res_data;
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  scoped_refptr<SafeBrowsingDatabaseManager> db_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SafeBrowsingDatabaseManagerTest, CheckApiBlocklistUrlWrongScheme) {
  EXPECT_TRUE(
      db_manager_->CheckApiBlocklistUrl(GURL("file://example.txt"), nullptr));
}

TEST_F(SafeBrowsingDatabaseManagerTest, CancelApiCheck) {
  TestClient client;
  const GURL url("https://www.example.com/more");

  EXPECT_FALSE(db_manager_->CheckApiBlocklistUrl(url, &client));
  EXPECT_TRUE(db_manager_->CancelApiCheck(&client));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(client.callback_invoked());
  EXPECT_EQ(0ul, client.GetBlockedPermissions().size());
}

TEST_F(SafeBrowsingDatabaseManagerTest, GetApiCheckResponse) {
  TestClient client;
  const GURL url("https://www.example.com/more");

  GURL request_url;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_url = request.url;
      }));

  EXPECT_FALSE(db_manager_->CheckApiBlocklistUrl(url, &client));
  test_url_loader_factory_.AddResponse(request_url.spec(),
                                       GetStockV4GetHashResponse());
  base::RunLoop().RunUntilIdle();

  client.WaitForCallback();
  ASSERT_EQ(1ul, client.GetBlockedPermissions().size());
  EXPECT_EQ("GEOLOCATION", *(client.GetBlockedPermissions().begin()));
}

}  // namespace safe_browsing
