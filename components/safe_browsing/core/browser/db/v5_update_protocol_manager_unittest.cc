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
#include "base/test/metrics/histogram_tester.h"
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
  using ParsedResponse = V5UpdateProtocolManager::ParsedResponse;
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
  base::expected<V5UpdateProtocolManager::ParsedResponse,
                 V5UpdateProtocolManager::V5ParseResult>
  ParseUpdateResponse(
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
      const std::vector<ExpectedV5Update>& expected_updates,
      std::optional<base::TimeDelta> minimum_wait_duration =
          std::nullopt) const {
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

      if (minimum_wait_duration.has_value()) {
        V5::Duration* duration = hash_list->mutable_minimum_wait_duration();
        duration->set_seconds(minimum_wait_duration->InSeconds());
        duration->set_nanos(0);
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

TEST_F(V5UpdateProtocolManagerTest, TestGetUpdatesErrorHandlingNetwork) {
  base::HistogramTester histogram_tester;
  const std::vector<ExpectedV5Update> expected_updates;
  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = false;

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  network::URLLoaderCompletionStatus status(net::ERR_CONNECTION_RESET);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url, status,
      network::mojom::URLResponseHead::New(), "");

  EXPECT_EQ(1ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kNetworkError, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Result",
                                      V4OperationResult::NETWORK_ERROR, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.Result",
                                      net::ERR_CONNECTION_RESET, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Network.Result",
                                      net::ERR_CONNECTION_RESET, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.V5Update.Network.Time", 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.TimedOut",
                                      false, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBUpdate.Network.Time", 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Network.TimedOut",
                                      false, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestGetUpdatesErrorHandlingTimeout) {
  base::HistogramTester histogram_tester;
  const std::vector<ExpectedV5Update> expected_updates;
  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = false;

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  // Update should start running within 5 minutes.
  bool update_fired = false;
  for (int i = 0; i < 5; ++i) {
    task_environment_.FastForwardBy(base::Minutes(1));
    if (!IsUpdateScheduled(pm.get())) {
      update_fired = true;
      break;
    }
  }
  EXPECT_TRUE(update_fired);
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  // Update should time out after 15 minutes.
  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  EXPECT_EQ(1ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kNetworkError, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Result",
                                      V4OperationResult::NETWORK_ERROR, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.Result",
                                      net::ERR_TIMED_OUT, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBUpdate.Network.Result", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.V5Update.Network.Time", 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.TimedOut",
                                      true, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBUpdate.Network.Time", 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Network.TimedOut",
                                      true, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestGetUpdatesErrorHandlingResponseCode) {
  base::HistogramTester histogram_tester;
  const std::vector<ExpectedV5Update> expected_updates;
  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = false;

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      std::string(), net::HTTP_NO_CONTENT);

  EXPECT_EQ(1ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kHttpError, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Result",
                                      V4OperationResult::HTTP_ERROR, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.Result",
                                      net::HTTP_NO_CONTENT, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Network.Result",
                                      net::HTTP_NO_CONTENT, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestGetUpdatesWithOneBackoff) {
  std::vector<ExpectedV5Update> expected_updates;
  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  ListIdentifier uws(SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
  expected_updates.push_back({malware, "new_state_1", true});
  expected_updates.push_back({uws, "new_state_2", true});

  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = false;

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  // Fail the first update.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      std::string(), net::HTTP_NO_CONTENT);

  // One error detected but still same multiplier.
  EXPECT_EQ(1ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  // New update automatically scheduled.
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  expect_callback_to_be_called_ = true;
  task_environment_.FastForwardBy(base::Minutes(14));
  // Backoff still going for at least the first 15 minutes.
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  // Backoff completes after maximum 30 minutes.
  bool update_fired = false;
  for (int i = 0; i < 16; ++i) {
    task_environment_.FastForwardBy(base::Minutes(1));
    if (!IsUpdateScheduled(pm.get())) {
      update_fired = true;
      break;
    }
  }
  EXPECT_TRUE(update_fired);

  // Succeed on second update.
  std::string response_data = GetExpectedV5UpdateResponse(expected_updates);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);

  // Error goes back to 0.
  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  // No new update automatically scheduled.
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
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
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Result",
                                      V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.Result",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Network.Result",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.V5Update.Network.Time", 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.Network.TimedOut",
                                      false, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBUpdate.Network.Time", 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.Network.TimedOut",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Update.ResponseSizeKB",
                                      response_data.size() / 1024, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBUpdate.ResponseSizeKB",
                                      response_data.size() / 1024, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kSuccess, 1);

  // Teardown will confirm the callback was called.
}

TEST_F(V5UpdateProtocolManagerTest, TestPostProcessingTimeAdjustment) {
  base::HistogramTester histogram_tester;
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

  // Verify histogram logging
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBUpdate.NextUpdateInterval", 60000, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.NextUpdateInterval", 60000, 1);
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingMinimumWaitDuration) {
  struct TestCase {
    std::string test_case_name;
    std::vector<std::pair<ListIdentifier, std::optional<int>>>
        lists_and_durations;
    base::expected<int, V5UpdateProtocolManager::V5ParseResult>
        expected_duration_or_error;
    std::optional<bool> has_unexpected_wait_duration;
  };

  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  ListIdentifier uws(SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
  ListIdentifier phishing(SBThreatType::SB_THREAT_TYPE_URL_PHISHING);

  std::vector<TestCase> test_cases = {
      {"Valid wait duration",
       {{malware, 60}},
       60,
       /*has_unexpected_wait_duration*/ false},
      {"Unset wait duration",
       {{malware, std::nullopt}},
       0,
       /*has_unexpected_wait_duration*/ true},
      {"Zero wait duration",
       {{malware, 0}},
       0,
       /*has_unexpected_wait_duration*/ true},
      {"Negative wait duration",
       {{malware, -60}},
       base::unexpected(
           V5UpdateProtocolManager::V5ParseResult::kNegativeDurationError),
       /*has_unexpected_wait_duration*/ std::nullopt},
      {"Multiple wait durations",
       {{malware, 120}, {uws, 60}, {phishing, 180}},
       60,
       /*has_unexpected_wait_duration*/ false},
      {"Multiple wait durations with unset",
       {{malware, 120}, {uws, std::nullopt}, {phishing, 180}},
       0,
       /*has_unexpected_wait_duration*/ true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.test_case_name);
    base::HistogramTester histogram_tester;
    auto pm = CreateProtocolManager({});
    V5::BatchGetHashListsResponse response;
    std::vector<V5UpdateProtocolManager::ListIdentifierAndVersion> lists;

    for (const auto& [list_id, duration_opt] : test_case.lists_and_durations) {
      V5::HashList* hash_list = response.add_hash_lists();
      hash_list->set_name(GetV5ListName(list_id));
      if (duration_opt.has_value()) {
        V5::Duration* duration = hash_list->mutable_minimum_wait_duration();
        duration->set_seconds(*duration_opt);
        duration->set_nanos(0);
      }
      lists.push_back(
          V5UpdateProtocolManager::ListIdentifierAndVersion(list_id, "state"));
    }

    std::string response_data;
    response.SerializeToString(&response_data);
    base::expected<ParsedResponse, V5UpdateProtocolManager::V5ParseResult>
        parsed = ParseUpdateResponse(pm.get(), response_data, lists);
    if (test_case.expected_duration_or_error.has_value()) {
      ASSERT_TRUE(parsed.has_value());
      EXPECT_EQ(base::Seconds(test_case.expected_duration_or_error.value()),
                parsed->minimum_wait_duration);
    } else {
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(test_case.expected_duration_or_error.error(), parsed.error());
    }
    if (test_case.has_unexpected_wait_duration.has_value()) {
      histogram_tester.ExpectUniqueSample(
          "SafeBrowsing.V5Update.UnexpectedMinimumWaitDuration",
          test_case.has_unexpected_wait_duration.value(), 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "SafeBrowsing.V5Update.UnexpectedMinimumWaitDuration", 0);
    }
  }
}

TEST_F(V5UpdateProtocolManagerTest,
       TestBackToBackGetUpdatesWithoutWaitDuration) {
  store_state_map_->clear();
  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map_->insert({malware, "initial_state_1"});
  std::vector<ExpectedV5Update> expected_updates;
  expected_updates.push_back({malware, "new_state_1", true});

  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = true;

  // Update #1
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

  // Update #2
  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));

  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  // Teardown will confirm the callback was called.
}

TEST_F(V5UpdateProtocolManagerTest, TestBackToBackGetUpdatesWithWaitDuration) {
  store_state_map_->clear();
  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map_->insert({malware, "initial_state_1"});
  std::vector<ExpectedV5Update> expected_updates;
  expected_updates.push_back({malware, "new_state_1", true});

  auto pm(CreateProtocolManager(expected_updates));

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  expect_callback_to_be_called_ = true;

  // Update #1
  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  task_environment_.FastForwardBy(base::Minutes(10));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  std::string response_data =
      GetExpectedV5UpdateResponse(expected_updates, base::Minutes(10));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  // Update #2
  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));

  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  task_environment_.FastForwardBy(base::Minutes(9));
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));
  task_environment_.FastForwardBy(base::Minutes(2));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);

  EXPECT_EQ(0ul, GetUpdateErrorCount(pm.get()));
  EXPECT_EQ(1ul, GetUpdateBackOffMult(pm.get()));
  EXPECT_FALSE(IsUpdateScheduled(pm.get()));

  // Teardown will confirm the callback was called.
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingInvalidProto) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      "invalid_proto_data", net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kParseFromStringError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingMismatchedSize) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  // Request has 1 list.
  std::unique_ptr<StoreStateMap> small_store_state_map =
      std::make_unique<StoreStateMap>();
  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  small_store_state_map->insert({malware, "initial_state_1"});

  // Response has 2 lists.
  V5::BatchGetHashListsResponse response;
  V5::HashList* list1 = response.add_hash_lists();
  list1->set_name(GetV5ListName(malware));
  list1->set_version("new_state_1");
  list1->set_partial_update(false);

  V5::HashList* list2 = response.add_hash_lists();
  ListIdentifier uws(SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
  list2->set_name(GetV5ListName(uws));
  list2->set_version("new_state_2");
  list2->set_partial_update(false);

  std::string response_data;
  response.SerializeToString(&response_data);

  pm->ScheduleNextUpdate(std::move(small_store_state_map));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kMismatchedSizeError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingMismatchedName) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map_ =
      std::make_unique<std::unordered_map<ListIdentifier, std::string>>();
  (*store_state_map_)[malware] = "state";

  V5::BatchGetHashListsResponse response;
  V5::HashList* hash_list = response.add_hash_lists();
  ListIdentifier uws(SBThreatType::SB_THREAT_TYPE_URL_UNWANTED);
  hash_list->set_name(GetV5ListName(uws));

  std::string response_data;
  response.SerializeToString(&response_data);

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kMismatchedNameError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingEmptyName) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map_ =
      std::make_unique<std::unordered_map<ListIdentifier, std::string>>();
  (*store_state_map_)[malware] = "state";

  V5::BatchGetHashListsResponse response;
  V5::HashList* hash_list = response.add_hash_lists();
  hash_list->set_name("");

  std::string response_data;
  response.SerializeToString(&response_data);

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kMismatchedNameError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingMismatchedPrefixLength) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  ListIdentifier malware(SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  store_state_map_ =
      std::make_unique<std::unordered_map<ListIdentifier, std::string>>();
  (*store_state_map_)[malware] = "state";

  V5::BatchGetHashListsResponse response;
  V5::HashList* hash_list = response.add_hash_lists();
  hash_list->set_name(GetV5ListName(malware));

  hash_list->mutable_additions_eight_bytes();

  std::string response_data;
  response.SerializeToString(&response_data);

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kMismatchedPrefixLengthError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest,
       TestResponseParsingMismatchedPrefixLengthAlternate) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  ListIdentifier csd(SBThreatType::SB_THREAT_TYPE_CSD_ALLOWLIST);
  std::unique_ptr<StoreStateMap> csd_store_state_map =
      std::make_unique<StoreStateMap>();
  csd_store_state_map->insert({csd, "state"});

  V5::BatchGetHashListsResponse response;
  V5::HashList* hash_list = response.add_hash_lists();
  hash_list->set_name(GetV5ListName(csd));
  // csd expects 32 byte prefix length. Add 16 bytes additions.
  hash_list->mutable_additions_sixteen_bytes();

  std::string response_data;
  response.SerializeToString(&response_data);

  pm->ScheduleNextUpdate(std::move(csd_store_state_map));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_data, net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kMismatchedPrefixLengthError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

TEST_F(V5UpdateProtocolManagerTest, TestResponseParsingNullBody) {
  base::HistogramTester histogram_tester;
  expect_callback_to_be_called_ = false;
  auto pm = CreateProtocolManager({}, /*expect_success=*/false);

  pm->ScheduleNextUpdate(std::make_unique<StoreStateMap>(*store_state_map_));
  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(IsUpdateScheduled(pm.get()));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(), "",
      net::HTTP_OK);
  EXPECT_TRUE(IsUpdateScheduled(pm.get()));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Parse.Result",
      V5UpdateProtocolManager::V5ParseResult::kParseFromStringError, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Update.Result",
      V5UpdateProtocolManager::V5OperationResult::kParseError, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::STATUS_200, 1);
  histogram_tester.ExpectBucketCount("SafeBrowsing.SBUpdate.Result",
                                     V4OperationResult::PARSE_ERROR, 1);

  // Teardown will confirm the callback was not called.
}

}  // namespace safe_browsing
