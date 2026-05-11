// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_update_protocol_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace safe_browsing {

struct ExpectedV5Update {
  ListIdentifier list_id;
  std::string version;
  bool partial_update;
};

class V5UpdateProtocolManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(safe_browsing::kLocalListsUseSBv5);

    store_state_map_ = std::make_unique<StoreStateMap>();
    ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
    store_state_map_->insert({malware, "initial_state_1"});
    ListIdentifier uws(SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
    store_state_map_->insert({uws, "initial_state_2"});

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override {
    if (expect_callback_to_be_called_) {
      EXPECT_TRUE(callback_was_called_);
    }
    PlatformTest::TearDown();
  }

 protected:
  std::string GetBase64SerializedUpdateRequestProto(
      const std::vector<V5UpdateProtocolManager::ListIdentifierAndVersion>&
          mapping) {
    return V5UpdateProtocolManager::GetBase64SerializedUpdateRequestProto(
        mapping);
  }
  V5UpdateProtocolManager::ParsedResponse ParseUpdateResponse(
      V5UpdateProtocolManager* pm,
      const std::optional<std::string>& response_body,
      const std::vector<V5UpdateProtocolManager::ListIdentifierAndVersion>&
          mapping) {
    return pm->ParseUpdateResponse(response_body, mapping);
  }
  base::TimeDelta GetNextUpdateInterval(V5UpdateProtocolManager* pm) {
    return pm->next_update_interval_;
  }
  base::TimeDelta GetNextUpdateIntervalWithBackoff(V5UpdateProtocolManager* pm,
                                                   bool back_off) {
    return pm->GetNextUpdateInterval(back_off);
  }
  void SetNextUpdateInterval(V5UpdateProtocolManager* pm,
                             base::TimeDelta interval) {
    pm->next_update_interval_ = interval;
  }
  void SetLastResponseTime(V5UpdateProtocolManager* pm, base::Time time) {
    pm->last_response_time_ = time;
  }
  size_t GetUpdateErrorCount(V5UpdateProtocolManager* pm) {
    return pm->update_error_count_;
  }
  size_t GetUpdateBackOffMult(V5UpdateProtocolManager* pm) {
    return pm->update_back_off_mult_;
  }
  bool IsUpdateScheduled(V5UpdateProtocolManager* pm) {
    return pm->IsUpdateScheduled();
  }

  bool HasPendingRequest(V5UpdateProtocolManager* pm) {
    return pm->request_ != nullptr;
  }

  void ValidateV5UpdateResults(
      const std::vector<ExpectedV5Update>& expected_updates,
      bool expect_success,
      std::optional<std::map<ListIdentifier, V5::HashList>> response) {
    callback_was_called_ = true;
    EXPECT_TRUE(expect_callback_to_be_called_);
    EXPECT_EQ(expect_success, response.has_value());
    if (!expect_success) {
      return;
    }
    EXPECT_EQ(expected_updates.size(), response->size());
    for (const auto& update : expected_updates) {
      auto it = response->find(update.list_id);
      ASSERT_TRUE(it != response->end());
      EXPECT_EQ(update.version, it->second.version());
      EXPECT_EQ(update.partial_update, it->second.partial_update());
    }
  }

  std::unique_ptr<V5UpdateProtocolManager> CreateProtocolManager(
      const std::vector<ExpectedV5Update>& expected_updates,
      bool expect_success = true,
      bool disable_auto_update = false) {
    return std::make_unique<V5UpdateProtocolManager>(
        test_shared_loader_factory_,
        GetTestV4ProtocolConfig(disable_auto_update),
        base::BindRepeating(
            &V5UpdateProtocolManagerTest::ValidateV5UpdateResults,
            base::Unretained(this), expected_updates, expect_success));
  }

  // Builds the expected response. Any list in `store_state_map_` gets
  // a response; if it's not specified in `expected_updates` then it
  // gets a default response.
  std::string GetExpectedV5UpdateResponse(
      const std::vector<ExpectedV5Update>& expected_updates) const {
    V5::BatchGetHashListsResponse response;
    for (const auto& [list_id, client_version] : *store_state_map_) {
      V5::HashList* hash_list = response.add_hash_lists();
      hash_list->set_name(GetV5ListName(list_id));

      const ExpectedV5Update* matching_update = nullptr;
      for (const auto& update : expected_updates) {
        if (update.list_id == list_id) {
          matching_update = &update;
          break;
        }
      }

      if (matching_update) {
        hash_list->set_version(matching_update->version);
        hash_list->set_partial_update(matching_update->partial_update);
      } else {
        hash_list->set_version("new_state");
        hash_list->set_partial_update(false);
      }
    }
    std::string res_data;
    response.SerializeToString(&res_data);
    return res_data;
  }

  bool expect_callback_to_be_called_ = false;
  bool callback_was_called_ = false;
  std::unique_ptr<StoreStateMap> store_state_map_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(V5UpdateProtocolManagerTest, TestDisableAutoUpdates) {
  auto pm = CreateProtocolManager(std::vector<ExpectedV5Update>(),
                                  /*expect_success=*/true,
                                  /*disable_auto_update=*/true);

  pm->ScheduleNextUpdate(std::move(store_state_map_));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  DCHECK(!HasPendingRequest(pm.get()));
}

TEST_F(V5UpdateProtocolManagerTest, TestEnableAutoUpdates) {
  auto pm = CreateProtocolManager(std::vector<ExpectedV5Update>(),
                                  /*expect_success=*/true,
                                  /*disable_auto_update=*/false);

  pm->ScheduleNextUpdate(std::move(store_state_map_));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  DCHECK(!HasPendingRequest(pm.get()));
}

TEST_F(V5UpdateProtocolManagerTest, TestBase64EncodingUsesUrlEncoding) {
  // Picked by generating random strings until one led to a '-' in the base64
  // url encoded request output.
  std::string magic_string = "ANo>Qqel>C";
  auto pm = CreateProtocolManager({});
  std::unique_ptr<StoreStateMap> store_state_map =
      std::make_unique<StoreStateMap>();
  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map->insert({malware, magic_string});
  std::vector<V5UpdateProtocolManager::ListIdentifierAndVersion> mapping;
  mapping.push_back(
      V5UpdateProtocolManager::ListIdentifierAndVersion(malware, magic_string));
  std::string encoded_request_with_minus =
      GetBase64SerializedUpdateRequestProto(mapping);
  std::string expected = "CgVtdy00YhIKQU5vPlFxZWw-Qw==";
  ASSERT_TRUE(encoded_request_with_minus.find("-") != std::string::npos);
  EXPECT_EQ(expected, encoded_request_with_minus);
}

TEST_F(V5UpdateProtocolManagerTest, TestGetUpdatesNoError) {
  store_state_map_->clear();
  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map_->insert({malware, "initial_state_1"});
  std::vector<ExpectedV5Update> expected_updates;
  expected_updates.push_back({malware, "new_state_1", true});

  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = true;

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  task_environment_.FastForwardBy(base::Minutes(10));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  std::string response_data = GetExpectedV5UpdateResponse(expected_updates);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  // Teardown will confirm the callback was called.
}

TEST_F(V5UpdateProtocolManagerTest, TestPostProcessingTimeAdjustment) {
  auto pm = CreateProtocolManager({});

  // Set initial conditions.
  SetNextUpdateInterval(pm.get(), base::Seconds(60));
  SetLastResponseTime(pm.get(), base::Time::Now());

  // Simulate 10 seconds of post-processing time.
  task_environment_.FastForwardBy(base::Seconds(10));

  // Call GetNextUpdateIntervalWithBackoff.
  base::TimeDelta interval =
      GetNextUpdateIntervalWithBackoff(pm.get(), /*back_off=*/false);

  // Verify it is adjusted.
  EXPECT_EQ(base::Seconds(50), interval);
}

}  // namespace safe_browsing
