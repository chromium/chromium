// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_ui_service.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_logger.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

// Dummy method for creating TestPreviewsUIService.
bool MockedPreviewsIsEnabled(previews::PreviewsType type) {
  return true;
}

class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
      std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
      std::unique_ptr<PreviewsLogger> logger,
      network::TestNetworkQualityTracker* test_network_quality_tracker)
      : PreviewsUIService(std::move(previews_decider_impl),
                          std::move(previews_opt_out_store),
                          std::move(previews_opt_guide),
                          base::BindRepeating(&MockedPreviewsIsEnabled),
                          std::move(logger),
                          blacklist::BlacklistData::AllowedTypesAndVersions(),
                          test_network_quality_tracker) {}
  ~TestPreviewsUIService() override {}
};

// Mock class of PreviewsLogger for checking passed in parameters.
class TestPreviewsLogger : public PreviewsLogger {
 public:
  TestPreviewsLogger()
      : decision_page_id_(0),
        navigation_opt_out_(false),
        user_blacklisted_(false),
        blacklist_ignored_(false) {}

  // PreviewsLogger:
  void LogPreviewNavigation(const GURL& url,
                            PreviewsType type,
                            bool opt_out,
                            base::Time time,
                            uint64_t page_id) override {
    navigation_url_ = url;
    navigation_opt_out_ = opt_out;
    navigation_type_ = type;
    navigation_time_ = base::Time(time);
    navigation_page_id_ = page_id;
  }

  void LogPreviewDecisionMade(
      PreviewsEligibilityReason reason,
      const GURL& url,
      base::Time time,
      PreviewsType type,
      std::vector<PreviewsEligibilityReason>&& passed_reasons,
      uint64_t page_id) override {
    decision_reason_ = reason;
    decision_url_ = GURL(url);
    decision_time_ = time;
    decision_type_ = type;
    decision_passed_reasons_ = std::move(passed_reasons);
    decision_page_id_ = page_id;
  }

  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
    host_blacklisted_ = host;
    host_blacklisted_time_ = time;
  }

  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    user_blacklisted_ = blacklisted;
  }

  void OnBlacklistCleared(base::Time time) override {
    blacklist_cleared_time_ = time;
  }

  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Return the passed in LogPreviewDecision parameters.
  PreviewsEligibilityReason decision_reason() const { return decision_reason_; }
  GURL decision_url() const { return decision_url_; }
  PreviewsType decision_type() const { return decision_type_; }
  base::Time decision_time() const { return decision_time_; }
  const std::vector<PreviewsEligibilityReason>& decision_passed_reasons()
      const {
    return decision_passed_reasons_;
  }
  uint64_t decision_page_id() const { return decision_page_id_; }

  // Return the passed in LogPreviewNavigation parameters.
  GURL navigation_url() const { return navigation_url_; }
  bool navigation_opt_out() const { return navigation_opt_out_; }
  base::Time navigation_time() const { return navigation_time_; }
  PreviewsType navigation_type() const { return navigation_type_; }
  uint64_t navigation_page_id() const { return navigation_page_id_; }

  // Return the passed in OnBlacklist events.
  std::string host_blacklisted() const { return host_blacklisted_; }
  base::Time host_blacklisted_time() const { return host_blacklisted_time_; }
  bool user_blacklisted() const { return user_blacklisted_; }
  base::Time blacklist_cleared_time() const { return blacklist_cleared_time_; }

  // Return the status of blacklist ignored.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  // Passed in LogPreviewDecision parameters.
  PreviewsEligibilityReason decision_reason_;
  GURL decision_url_;
  PreviewsType decision_type_;
  base::Time decision_time_;
  std::vector<PreviewsEligibilityReason> decision_passed_reasons_;
  uint64_t decision_page_id_;

  // Passed in LogPreviewsNavigation parameters.
  GURL navigation_url_;
  bool navigation_opt_out_;
  base::Time navigation_time_;
  PreviewsType navigation_type_;
  uint64_t navigation_page_id_;

  // Passed in OnBlacklist events.
  std::string host_blacklisted_;
  base::Time host_blacklisted_time_;
  bool user_blacklisted_;
  base::Time blacklist_cleared_time_;

  // Passed in blacklist ignored status.
  bool blacklist_ignored_;
};

class TestPreviewsDeciderImpl : public PreviewsDeciderImpl {
 public:
  TestPreviewsDeciderImpl()
      : PreviewsDeciderImpl(base::DefaultClock::GetInstance()),
        blacklist_ignored_(false) {}

  // PreviewsDeciderImpl:
  void SetIgnorePreviewsBlacklistDecision(bool ignored) override {
    blacklist_ignored_ = ignored;
  }
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const override {
    if (url.host() == "blockresources.com") {
      out_resource_patterns_to_block->push_back("BlockMe");
      return true;
    }
    return false;
  }

  // Exposed the status of blacklist decisions ignored for testing
  // PreviewsUIService.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  // Whether the blacklist decisions are ignored or not.
  bool blacklist_ignored_;
};

class PreviewsUIServiceTest : public testing::Test {
 public:
  PreviewsUIServiceTest() {}

  ~PreviewsUIServiceTest() override {}

  void SetUp() override {
    std::unique_ptr<TestPreviewsLogger> logger =
        std::make_unique<TestPreviewsLogger>();

    // Use to testing logger data.
    logger_ptr_ = logger.get();

    std::unique_ptr<TestPreviewsDeciderImpl> previews_decider_impl =
        std::make_unique<TestPreviewsDeciderImpl>();
    previews_decider_impl_ = previews_decider_impl.get();

    ui_service_ = std::make_unique<TestPreviewsUIService>(
        std::move(previews_decider_impl), nullptr /* previews_opt_out_store */,
        nullptr /* previews_opt_guide */, std::move(logger),
        &test_network_quality_tracker_);
  }

  TestPreviewsDeciderImpl* previews_decider_impl() {
    return previews_decider_impl_;
  }

  TestPreviewsUIService* ui_service() { return ui_service_.get(); }

 protected:
  // Run this test on a single thread.
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestPreviewsLogger* logger_ptr_;
  network::TestNetworkQualityTracker test_network_quality_tracker_;

 private:
  TestPreviewsDeciderImpl* previews_decider_impl_;
  std::unique_ptr<TestPreviewsUIService> ui_service_;
};

}  // namespace

TEST_F(PreviewsUIServiceTest, TestInitialization) {
  // After the outstanding posted tasks have run, SetIOData should have been
  // called for |ui_service_|.
  EXPECT_TRUE(ui_service()->previews_decider_impl());
}

TEST_F(PreviewsUIServiceTest, TestLogPreviewNavigationPassInCorrectParams) {
  const GURL url_a = GURL("http://www.url_a.com/url_a");
  const PreviewsType type_a = PreviewsType::LITE_PAGE;
  const bool opt_out_a = true;
  const base::Time time_a = base::Time::Now();
  const uint64_t page_id_a = 1234;

  ui_service()->LogPreviewNavigation(url_a, type_a, opt_out_a, time_a,
                                     page_id_a);

  EXPECT_EQ(url_a, logger_ptr_->navigation_url());
  EXPECT_EQ(type_a, logger_ptr_->navigation_type());
  EXPECT_EQ(opt_out_a, logger_ptr_->navigation_opt_out());
  EXPECT_EQ(time_a, logger_ptr_->navigation_time());
  EXPECT_EQ(page_id_a, logger_ptr_->navigation_page_id());

  const GURL url_b = GURL("http://www.url_b.com/url_b");
  const PreviewsType type_b = PreviewsType::OFFLINE;
  const bool opt_out_b = false;
  const base::Time time_b = base::Time::Now();
  const uint64_t page_id_b = 4321;

  ui_service()->LogPreviewNavigation(url_b, type_b, opt_out_b, time_b,
                                     page_id_b);

  EXPECT_EQ(url_b, logger_ptr_->navigation_url());
  EXPECT_EQ(type_b, logger_ptr_->navigation_type());
  EXPECT_EQ(opt_out_b, logger_ptr_->navigation_opt_out());
  EXPECT_EQ(time_b, logger_ptr_->navigation_time());
  EXPECT_EQ(page_id_b, logger_ptr_->navigation_page_id());
}

TEST_F(PreviewsUIServiceTest, TestLogPreviewDecisionMadePassesCorrectParams) {
  PreviewsEligibilityReason reason_a =
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  const GURL url_a("http://www.url_a.com/url_a");
  const base::Time time_a = base::Time::Now();
  PreviewsType type_a = PreviewsType::OFFLINE;
  std::vector<PreviewsEligibilityReason> passed_reasons_a = {
      PreviewsEligibilityReason::NETWORK_NOT_SLOW,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
  };
  const std::vector<PreviewsEligibilityReason> expected_passed_reasons_a(
      passed_reasons_a);
  const uint64_t page_id_a = 1234;

  ui_service()->LogPreviewDecisionMade(reason_a, url_a, time_a, type_a,
                                       std::move(passed_reasons_a), page_id_a);

  EXPECT_EQ(reason_a, logger_ptr_->decision_reason());
  EXPECT_EQ(url_a, logger_ptr_->decision_url());
  EXPECT_EQ(time_a, logger_ptr_->decision_time());
  EXPECT_EQ(type_a, logger_ptr_->decision_type());
  EXPECT_EQ(expected_passed_reasons_a, logger_ptr_->decision_passed_reasons());
  EXPECT_EQ(page_id_a, logger_ptr_->decision_page_id());

  auto actual_passed_reasons_a = logger_ptr_->decision_passed_reasons();
  EXPECT_EQ(3UL, actual_passed_reasons_a.size());
  for (size_t i = 0; i < actual_passed_reasons_a.size(); i++) {
    EXPECT_EQ(expected_passed_reasons_a[i], actual_passed_reasons_a[i]);
  }

  PreviewsEligibilityReason reason_b =
      PreviewsEligibilityReason::NETWORK_NOT_SLOW;
  const GURL url_b("http://www.url_b.com/url_b");
  const base::Time time_b = base::Time::Now();
  PreviewsType type_b = PreviewsType::OFFLINE;
  std::vector<PreviewsEligibilityReason> passed_reasons_b = {
      PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE,
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
  };
  const std::vector<PreviewsEligibilityReason> expected_passed_reasons_b(
      passed_reasons_b);
  const uint64_t page_id_b = 4321;

  ui_service()->LogPreviewDecisionMade(reason_b, url_b, time_b, type_b,
                                       std::move(passed_reasons_b), page_id_b);

  EXPECT_EQ(reason_b, logger_ptr_->decision_reason());
  EXPECT_EQ(url_b, logger_ptr_->decision_url());
  EXPECT_EQ(type_b, logger_ptr_->decision_type());
  EXPECT_EQ(time_b, logger_ptr_->decision_time());
  EXPECT_EQ(page_id_b, logger_ptr_->decision_page_id());

  auto actual_passed_reasons_b = logger_ptr_->decision_passed_reasons();
  EXPECT_EQ(2UL, actual_passed_reasons_b.size());
  for (size_t i = 0; i < actual_passed_reasons_b.size(); i++) {
    EXPECT_EQ(expected_passed_reasons_b[i], actual_passed_reasons_b[i]);
  }
}

TEST_F(PreviewsUIServiceTest, TestOnNewBlacklistedHostPassesCorrectParams) {
  const std::string expected_host = "example.com";
  const base::Time expected_time = base::Time::Now();
  ui_service()->OnNewBlacklistedHost(expected_host, expected_time);

  EXPECT_EQ(expected_host, logger_ptr_->host_blacklisted());
  EXPECT_EQ(expected_time, logger_ptr_->host_blacklisted_time());
}

TEST_F(PreviewsUIServiceTest, TestOnUserBlacklistedPassesCorrectParams) {
  ui_service()->OnUserBlacklistedStatusChange(true /* blacklisted */);
  EXPECT_TRUE(logger_ptr_->user_blacklisted());

  ui_service()->OnUserBlacklistedStatusChange(false /* blacklisted */);
  EXPECT_FALSE(logger_ptr_->user_blacklisted());
}

TEST_F(PreviewsUIServiceTest, TestOnBlacklistClearedPassesCorrectParams) {
  const base::Time expected_time = base::Time::Now();
  ui_service()->OnBlacklistCleared(expected_time);

  EXPECT_EQ(expected_time, logger_ptr_->blacklist_cleared_time());
}

TEST_F(PreviewsUIServiceTest,
       TestSetIgnorePreviewsBlacklistDecisionPassesCorrectParams) {
  ui_service()->SetIgnorePreviewsBlacklistDecision(true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(previews_decider_impl()->blacklist_ignored());

  ui_service()->SetIgnorePreviewsBlacklistDecision(false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(previews_decider_impl()->blacklist_ignored());
}

TEST_F(PreviewsUIServiceTest, TestOnIgnoreBlacklistDecisionStatusChanged) {
  ui_service()->OnIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
  EXPECT_TRUE(logger_ptr_->blacklist_ignored());

  ui_service()->OnIgnoreBlacklistDecisionStatusChanged(false /* ignored */);
  EXPECT_FALSE(logger_ptr_->blacklist_ignored());
}

TEST_F(PreviewsUIServiceTest,
       TestGetResourceLoadingHintsResourcePatternsToBlock) {
  EXPECT_TRUE(ui_service()
                  ->GetResourceLoadingHintsResourcePatternsToBlock(
                      GURL("https://www.somedomain.org/"))
                  .empty());

  std::vector<std::string> patterns_to_block =
      ui_service()->GetResourceLoadingHintsResourcePatternsToBlock(
          GURL("https://blockresources.com/"));
  EXPECT_EQ(1ul, patterns_to_block.size());
  EXPECT_EQ("BlockMe", patterns_to_block[0]);
}

}  // namespace previews
