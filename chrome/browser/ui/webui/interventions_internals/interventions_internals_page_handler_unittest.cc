// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_logger_observer.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/features.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The HTML DOM ID used in Javascript.
constexpr char kPreviewsAllowedHtmlId[] = "previews-allowed-status";
constexpr char kOfflinePreviewsHtmlId[] = "offline-preview-status";
constexpr char kLitePageRedirectHtmlId[] = "lite-page-redirect-status";
constexpr char kResourceLoadingHintsHtmlId[] = "resource-loading-hints-status";
constexpr char kDeferAllScriptPreviewsHtmlId[] =
    "defer-all-script-preview-status";
constexpr char kNoScriptPreviewsHtmlId[] = "noscript-preview-status";

// Descriptions for previews.
constexpr char kPreviewsAllowedDescription[] = "Previews Allowed";
constexpr char kOfflineDesciption[] = "Offline Previews";
constexpr char kLitePageRedirectDescription[] =
    "Lite Page Redirect / Server Previews";
constexpr char kResourceLoadingHintsDescription[] =
    "ResourceLoadingHints Previews";
constexpr char kDeferAllScriptPreviewsDescription[] = "DeferAllScript Previews";
constexpr char kNoScriptDescription[] = "NoScript Previews";

// The HTML DOM ID used in Javascript.
constexpr char kOfflinePageFlagHtmlId[] = "offline-page-flag";
constexpr char kLitePageRedirectFlagHtmlId[] = "lite-page-redirect-flag";
constexpr char kResourceLoadingHintsFlagHtmlId[] =
    "resource-loading-hints-flag";
constexpr char kDeferAllScriptFlagHtmlId[] = "defer-all-script-flag";
constexpr char kNoScriptFlagHtmlId[] = "noscript-flag";
constexpr char kEctFlagHtmlId[] = "ect-flag";
constexpr char kIgnorePreviewsBlacklistFlagHtmlId[] =
    "ignore-previews-blacklist";
constexpr char kDataSaverAltConfigHtmlId[] =
    "data-reduction-proxy-server-experiment";

// Links to flags in chrome://flags.
constexpr char kOfflinePageFlagLink[] =
    "chrome://flags/#enable-offline-previews";
constexpr char kLitePageRedirectFlagLink[] =
    "chrome://flags/#enable-lite-page-server-previews";
constexpr char kResourceLoadingHintsFlagLink[] =
    "chrome://flags/#enable-resource-loading-hints";
constexpr char kDeferAllScriptFlagLink[] =
    "chrome://flags/#enable-defer-all-script";
constexpr char kNoScriptFlagLink[] = "chrome://flags/#enable-noscript-previews";
constexpr char kEctFlagLink[] =
    "chrome://flags/#force-effective-connection-type";
constexpr char kIgnorePreviewsBlacklistLink[] =
    "chrome://flags/#ignore-previews-blacklist";
constexpr char kDataSaverAltConfigLink[] =
    "chrome://flags/#enable-data-reduction-proxy-server-experiment";

// Flag features names.
constexpr char kOfflinePageFeatureName[] = "OfflinePreviews";
constexpr char kLitePageRedirectFeatureName[] = "LitePageServerPreviews";
constexpr char kResourceLoadingHintsFeatureName[] = "ResourceLoadingHints";
constexpr char kDeferAllScriptFeatureName[] = "DeferAllScriptPreviews";
constexpr char kNoScriptFeatureName[] = "NoScriptPreviews";

constexpr char kDefaultFlagValue[] = "Default";
constexpr char kEnabledFlagValue[] = "Enabled";
constexpr char kDisabledFlagValue[] = "Disabled";

// The map that would be passed to the callback in GetPreviewsEnabledCallback.
std::unordered_map<std::string, mojom::PreviewsStatusPtr> passed_in_modes;

// The map that would be passed to the callback in
// GetPreviewsFlagsDetailsCallback.
std::unordered_map<std::string, mojom::PreviewsFlagPtr> passed_in_flags;

// Mocked call back method to test GetPreviewsEnabledCallback.
void MockGetPreviewsEnabledCallback(
    std::vector<mojom::PreviewsStatusPtr> params) {
  passed_in_modes.clear();
  for (size_t i = 0; i < params.size(); i++) {
    passed_in_modes[params[i]->htmlId] = std::move(params[i]);
  }
}

// Mocked call back method to test GetPreviewsFlagsDetailsCallback.
void MockGetPreviewsFlagsCallback(std::vector<mojom::PreviewsFlagPtr> params) {
  passed_in_flags.clear();
  for (size_t i = 0; i < params.size(); i++) {
    passed_in_flags[params[i]->htmlId] = std::move(params[i]);
  }
}

// Dummy method for creating TestPreviewsUIService.
bool MockedPreviewsIsEnabled(previews::PreviewsType type) {
  return true;
}

// Mock class that would be pass into the InterventionsInternalsPageHandler by
// calling its SetClientPage method. Used to test that the PageHandler
// actually invokes the page's LogNewMessage method with the correct message.
class TestInterventionsInternalsPage
    : public mojom::InterventionsInternalsPage {
 public:
  TestInterventionsInternalsPage(
      mojo::PendingReceiver<mojom::InterventionsInternalsPage> receiver)
      : receiver_(this, std::move(receiver)), blacklist_ignored_(false) {}

  ~TestInterventionsInternalsPage() override {}

  // mojom::InterventionsInternalsPage:
  void LogNewMessage(mojom::MessageLogPtr message) override {
    message_ = std::make_unique<mojom::MessageLogPtr>(std::move(message));
  }
  void OnBlacklistedHost(const std::string& host, int64_t time) override {
    host_blacklisted_ = host;
    host_blacklisted_time_ = time;
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    user_blacklisted_ = blacklisted;
  }
  void OnBlacklistCleared(int64_t time) override {
    blacklist_cleared_time_ = time;
  }
  void UpdateEffectiveConnectionType(
      const std::string& type,
      const std::string& max_intervention_type) override {
    // Ignore.
    // TODO(thanhdle): Add integration test to test behavior of the pipeline end
    // to end. crbug.com/777936
  }
  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Expose passed in message in LogNewMessage for testing.
  mojom::MessageLogPtr* message() const { return message_.get(); }

  // Expose passed in blacklist events info for testing.
  std::string host_blacklisted() const { return host_blacklisted_; }
  int64_t host_blacklisted_time() const { return host_blacklisted_time_; }
  bool user_blacklisted() const { return user_blacklisted_; }
  int64_t blacklist_cleared_time() const { return blacklist_cleared_time_; }

  // Expose the passed in blacklist ignore status for testing.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  mojo::Receiver<mojom::InterventionsInternalsPage> receiver_;

  // The MessageLogPtr passed in LogNewMessage method.
  std::unique_ptr<mojom::MessageLogPtr> message_;

  // Received blacklist events info.
  std::string host_blacklisted_;
  int64_t host_blacklisted_time_;
  int64_t user_blacklisted_;
  int64_t blacklist_cleared_time_;

  // Whether to ignore previews blacklist decisions.
  bool blacklist_ignored_;
};

// Mock class to test interaction between the PageHandler and the
// PreviewsLogger.
class TestPreviewsLogger : public previews::PreviewsLogger {
 public:
  TestPreviewsLogger() : PreviewsLogger(), remove_is_called_(false) {}

  // PreviewsLogger:
  void RemoveObserver(previews::PreviewsLoggerObserver* obs) override {
    remove_is_called_ = true;
  }

  bool RemovedObserverIsCalled() const { return remove_is_called_; }

 private:
  bool remove_is_called_;
};

// A dummy class to setup PreviewsUIService.
class TestPreviewsDeciderImpl : public previews::PreviewsDeciderImpl {
 public:
  TestPreviewsDeciderImpl()
      : PreviewsDeciderImpl(base::DefaultClock::GetInstance()) {}

  // previews::PreviewsDeciderImpl:
  void Initialize(
      previews::PreviewsUIService* previews_ui_service,
      std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<previews::PreviewsOptimizationGuide> previews_opt_guide,
      const previews::PreviewsIsEnabledCallback& is_enabled_callback,
      blacklist::BlacklistData::AllowedTypesAndVersions allowed_previews)
      override {}
};

// Mocked TestPreviewsService for testing InterventionsInternalsPageHandler.
class TestPreviewsUIService : public previews::PreviewsUIService {
 public:
  TestPreviewsUIService(
      std::unique_ptr<previews::PreviewsDeciderImpl> previews_decider_impl,
      std::unique_ptr<previews::PreviewsLogger> logger,
      network::TestNetworkQualityTracker* test_network_quality_tracker)
      : previews::PreviewsUIService(
            std::move(previews_decider_impl),
            nullptr, /* previews_opt_out_store */
            nullptr, /* previews_opt_guide */
            base::BindRepeating(&MockedPreviewsIsEnabled),
            std::move(logger),
            blacklist::BlacklistData::AllowedTypesAndVersions(),
            test_network_quality_tracker),
        blacklist_ignored_(false) {}
  ~TestPreviewsUIService() override {}

  // previews::PreviewsUIService:
  void SetIgnorePreviewsBlacklistDecision(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Exposed blacklist ignored state.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  // Whether the blacklist decisions are ignored or not.
  bool blacklist_ignored_;
};

class InterventionsInternalsPageHandlerTest : public testing::Test {
 public:
  InterventionsInternalsPageHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ~InterventionsInternalsPageHandlerTest() override {}

  void SetUp() override {
    std::unique_ptr<TestPreviewsLogger> logger =
        std::make_unique<TestPreviewsLogger>();
    logger_ = logger.get();

    previews_ui_service_ = std::make_unique<TestPreviewsUIService>(
        std::make_unique<TestPreviewsDeciderImpl>(), std::move(logger),
        &test_network_quality_tracker_);

    ASSERT_TRUE(profile_manager_.SetUp());

    mojo::PendingReceiver<mojom::InterventionsInternalsPageHandler>
        handler_receiver =
            mojo::PendingRemote<mojom::InterventionsInternalsPageHandler>()
                .InitWithNewPipeAndPassReceiver();
    page_handler_ = std::make_unique<InterventionsInternalsPageHandler>(
        std::move(handler_receiver), previews_ui_service_.get(),
        &test_network_quality_tracker_);

    mojo::PendingRemote<mojom::InterventionsInternalsPage> page;
    mojo::PendingReceiver<mojom::InterventionsInternalsPage> page_receiver =
        page.InitWithNewPipeAndPassReceiver();
    page_ = std::make_unique<TestInterventionsInternalsPage>(
        std::move(page_receiver));

    page_handler_->SetClientPage(std::move(page));

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

  void TearDown() override {
    profile_manager_.DeleteAllTestingProfiles();
    page_handler_.reset();
    page_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;

 protected:
  TestingProfileManager profile_manager_;

  TestPreviewsLogger* logger_;
  network::TestNetworkQualityTracker test_network_quality_tracker_;
  std::unique_ptr<TestPreviewsUIService> previews_ui_service_;

  // InterventionsInternalPageHandler's variables.
  std::unique_ptr<InterventionsInternalsPageHandler> page_handler_;

  // InterventionsInternalPage's variables.
  std::unique_ptr<TestInterventionsInternalsPage> page_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(InterventionsInternalsPageHandlerTest, GetPreviewsEnabledCount) {
  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));

  constexpr size_t expected = 6;
  EXPECT_EQ(expected, passed_in_modes.size());
}

TEST_F(InterventionsInternalsPageHandlerTest, PreviewsAllowedDisabled) {
  // Init with kPreviews disabled.
  scoped_feature_list_->InitWithFeatures({}, {previews::features::kPreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto previews_allowed = passed_in_modes.find(kPreviewsAllowedHtmlId);
  ASSERT_NE(passed_in_modes.end(), previews_allowed);
  EXPECT_EQ(kPreviewsAllowedDescription, previews_allowed->second->description);
  EXPECT_FALSE(previews_allowed->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, PreviewsAllowedEnabled) {
  // Init with kPreviews enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kPreviews}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto previews_allowed = passed_in_modes.find(kPreviewsAllowedHtmlId);
  ASSERT_NE(passed_in_modes.end(), previews_allowed);
  EXPECT_EQ(kPreviewsAllowedDescription, previews_allowed->second->description);
  EXPECT_TRUE(previews_allowed->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, NoScriptDisabled) {
  // Init with kNoScript disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kNoScriptPreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto noscript = passed_in_modes.find(kNoScriptPreviewsHtmlId);
  ASSERT_NE(passed_in_modes.end(), noscript);
  EXPECT_EQ(kNoScriptDescription, noscript->second->description);
  EXPECT_FALSE(noscript->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, NoScriptEnabled) {
  // Init with kNoScript enabled.
  scoped_feature_list_->InitWithFeatures(
      {previews::features::kNoScriptPreviews}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto noscript = passed_in_modes.find(kNoScriptPreviewsHtmlId);
  ASSERT_NE(passed_in_modes.end(), noscript);
  EXPECT_EQ(kNoScriptDescription, noscript->second->description);
  EXPECT_TRUE(noscript->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, ResourceLoadingHintsDisabled) {
  // Init with kResourceLoadingHints disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kResourceLoadingHints});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto resource_loading_hints =
      passed_in_modes.find(kResourceLoadingHintsHtmlId);
  ASSERT_NE(passed_in_modes.end(), resource_loading_hints);
  EXPECT_EQ(kResourceLoadingHintsDescription,
            resource_loading_hints->second->description);
  EXPECT_FALSE(resource_loading_hints->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, ResourceLoadingHintsEnabled) {
  // Init with kResourceLoadingHints enabled.
  scoped_feature_list_->InitWithFeatures(
      {previews::features::kResourceLoadingHints}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto resource_loading_hints =
      passed_in_modes.find(kResourceLoadingHintsHtmlId);
  ASSERT_NE(passed_in_modes.end(), resource_loading_hints);
  EXPECT_EQ(kResourceLoadingHintsDescription,
            resource_loading_hints->second->description);
  EXPECT_TRUE(resource_loading_hints->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, OfflinePreviewsDisabled) {
  // Init with kOfflinePreviews disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kOfflinePreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto offline_previews = passed_in_modes.find(kOfflinePreviewsHtmlId);
  ASSERT_NE(passed_in_modes.end(), offline_previews);
  EXPECT_EQ(kOfflineDesciption, offline_previews->second->description);
  EXPECT_FALSE(offline_previews->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, OfflinePreviewsEnabled) {
  // Init with kOfflinePreviews enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kOfflinePreviews},
                                         {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto offline_previews = passed_in_modes.find(kOfflinePreviewsHtmlId);
  ASSERT_NE(passed_in_modes.end(), offline_previews);
  EXPECT_TRUE(offline_previews->second);
  EXPECT_EQ(kOfflineDesciption, offline_previews->second->description);
  EXPECT_TRUE(offline_previews->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, LitePageRedirectDisabled) {
  // Init with kLitePageRedirect disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kLitePageServerPreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto resource_loading_hints = passed_in_modes.find(kLitePageRedirectHtmlId);
  ASSERT_NE(passed_in_modes.end(), resource_loading_hints);
  EXPECT_EQ(kLitePageRedirectDescription,
            resource_loading_hints->second->description);
  EXPECT_FALSE(resource_loading_hints->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, LitePageRedirectEnabled) {
  // Init with kLitePageRedirect enabled.
  scoped_feature_list_->InitWithFeatures(
      {previews::features::kLitePageServerPreviews}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto resource_loading_hints = passed_in_modes.find(kLitePageRedirectHtmlId);
  ASSERT_NE(passed_in_modes.end(), resource_loading_hints);
  EXPECT_EQ(kLitePageRedirectDescription,
            resource_loading_hints->second->description);
  EXPECT_TRUE(resource_loading_hints->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, DeferAllScriptPreviewsDisabled) {
  // Init with kDeferAllScriptPreviews disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kDeferAllScriptPreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto defer_all_script = passed_in_modes.find(kDeferAllScriptPreviewsHtmlId);
  ASSERT_NE(passed_in_modes.end(), defer_all_script);
  EXPECT_EQ(kDeferAllScriptPreviewsDescription,
            defer_all_script->second->description);
  EXPECT_FALSE(defer_all_script->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, DeferAllScriptPreviewsEnabled) {
  // Init with kDeferAllScriptPreviews enabled.
  scoped_feature_list_->InitWithFeatures(
      {previews::features::kDeferAllScriptPreviews}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto defer_all_script = passed_in_modes.find(kDeferAllScriptPreviewsHtmlId);
  ASSERT_NE(passed_in_modes.end(), defer_all_script);
  EXPECT_EQ(kDeferAllScriptPreviewsDescription,
            defer_all_script->second->description);
  EXPECT_TRUE(defer_all_script->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsCount) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));

  constexpr size_t expected = 9;
  EXPECT_EQ(expected, passed_in_flags.size());
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsEctDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto ect_flag = passed_in_flags.find(kEctFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), ect_flag);
  EXPECT_EQ(flag_descriptions::kForceEffectiveConnectionTypeName,
            ect_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, ect_flag->second->value);
  EXPECT_EQ(kEctFlagLink, ect_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsForceEctValue) {
  std::string expected_ects[] = {
      net::kEffectiveConnectionTypeUnknown,
      net::kEffectiveConnectionTypeOffline,
      net::kEffectiveConnectionTypeSlow2G,
      net::kEffectiveConnectionType3G,
      net::kEffectiveConnectionType4G,
  };

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  for (auto expected_ect : expected_ects) {
    command_line->AppendSwitchASCII(
        network::switches::kForceEffectiveConnectionType, expected_ect);
    page_handler_->GetPreviewsFlagsDetails(
        base::BindOnce(&MockGetPreviewsFlagsCallback));
    auto ect_flag = passed_in_flags.find(kEctFlagHtmlId);

    ASSERT_NE(passed_in_flags.end(), ect_flag);
    EXPECT_EQ(flag_descriptions::kForceEffectiveConnectionTypeName,
              ect_flag->second->description);
    EXPECT_EQ(expected_ect, ect_flag->second->value);
    EXPECT_EQ(kEctFlagLink, ect_flag->second->link);
  }
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsEctForceFieldtrialValue) {
  const std::string trial_name = "NetworkQualityEstimator";
  const std::string group_name = "Enabled";
  const std::string expected_ect = "Slow-2G";

  std::map<std::string, std::string> params;
  params[net::kForceEffectiveConnectionType] = expected_ect;
  scoped_feature_list_->InitAndEnableFeatureWithParameters(
      net::features::kNetworkQualityEstimator, params);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));

  auto ect_flag = passed_in_flags.find(kEctFlagHtmlId);
  ASSERT_NE(passed_in_flags.end(), ect_flag);
  EXPECT_EQ(flag_descriptions::kForceEffectiveConnectionTypeName,
            ect_flag->second->description);
  EXPECT_EQ("Fieldtrial forced " + expected_ect, ect_flag->second->value);
  EXPECT_EQ(kEctFlagLink, ect_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       GetFlagsIgnorePreviewsBlacklistDisabledValue) {
  // Disabled by default.
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto ignore_previews_blacklist =
      passed_in_flags.find(kIgnorePreviewsBlacklistFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), ignore_previews_blacklist);
  EXPECT_EQ(flag_descriptions::kIgnorePreviewsBlacklistName,
            ignore_previews_blacklist->second->description);
  EXPECT_EQ(kDisabledFlagValue, ignore_previews_blacklist->second->value);
  EXPECT_EQ(kIgnorePreviewsBlacklistLink,
            ignore_previews_blacklist->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptDisabledValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto ignore_previews_blacklist =
      passed_in_flags.find(kIgnorePreviewsBlacklistFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), ignore_previews_blacklist);
  EXPECT_EQ(flag_descriptions::kIgnorePreviewsBlacklistName,
            ignore_previews_blacklist->second->description);
  EXPECT_EQ(kDisabledFlagValue, ignore_previews_blacklist->second->value);
  EXPECT_EQ(kIgnorePreviewsBlacklistLink,
            ignore_previews_blacklist->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto noscript_flag = passed_in_flags.find(kNoScriptFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), noscript_flag);
  EXPECT_EQ(flag_descriptions::kEnableNoScriptPreviewsName,
            noscript_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, noscript_flag->second->value);
  EXPECT_EQ(kNoScriptFlagLink, noscript_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptEnabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kNoScriptFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto noscript_flag = passed_in_flags.find(kNoScriptFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), noscript_flag);
  EXPECT_EQ(flag_descriptions::kEnableNoScriptPreviewsName,
            noscript_flag->second->description);
  EXPECT_EQ(kEnabledFlagValue, noscript_flag->second->value);
  EXPECT_EQ(kNoScriptFlagLink, noscript_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                  kNoScriptFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto noscript_flag = passed_in_flags.find(kNoScriptFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), noscript_flag);
  EXPECT_EQ(flag_descriptions::kEnableNoScriptPreviewsName,
            noscript_flag->second->description);
  EXPECT_EQ(kDisabledFlagValue, noscript_flag->second->value);
  EXPECT_EQ(kNoScriptFlagLink, noscript_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       GetFlagsResourceLoadingHintsDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto resource_loading_hints_flag =
      passed_in_flags.find(kResourceLoadingHintsFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), resource_loading_hints_flag);
  EXPECT_EQ(flag_descriptions::kEnableResourceLoadingHintsName,
            resource_loading_hints_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, resource_loading_hints_flag->second->value);
  EXPECT_EQ(kResourceLoadingHintsFlagLink,
            resource_loading_hints_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       GetFlagsResourceLoadingHintsEnabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kResourceLoadingHintsFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto resource_loading_hints_flag =
      passed_in_flags.find(kResourceLoadingHintsFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), resource_loading_hints_flag);
  EXPECT_EQ(flag_descriptions::kEnableResourceLoadingHintsName,
            resource_loading_hints_flag->second->description);
  EXPECT_EQ(kEnabledFlagValue, resource_loading_hints_flag->second->value);
  EXPECT_EQ(kResourceLoadingHintsFlagLink,
            resource_loading_hints_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       GetFlagsResourceLoadingHintsDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                  kResourceLoadingHintsFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto resource_loading_hints_flag =
      passed_in_flags.find(kResourceLoadingHintsFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), resource_loading_hints_flag);
  EXPECT_EQ(flag_descriptions::kEnableResourceLoadingHintsName,
            resource_loading_hints_flag->second->description);
  EXPECT_EQ(kDisabledFlagValue, resource_loading_hints_flag->second->value);
  EXPECT_EQ(kResourceLoadingHintsFlagLink,
            resource_loading_hints_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       GetFlagsDeferAllScriptDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto defer_all_script_flag = passed_in_flags.find(kDeferAllScriptFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), defer_all_script_flag);
  EXPECT_EQ(flag_descriptions::kEnableDeferAllScriptName,
            defer_all_script_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, defer_all_script_flag->second->value);
  EXPECT_EQ(kDeferAllScriptFlagLink, defer_all_script_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsDeferAllScriptEnabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kDeferAllScriptFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto defer_all_script_flag = passed_in_flags.find(kDeferAllScriptFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), defer_all_script_flag);
  EXPECT_EQ(flag_descriptions::kEnableDeferAllScriptName,
            defer_all_script_flag->second->description);
  EXPECT_EQ(kEnabledFlagValue, defer_all_script_flag->second->value);
  EXPECT_EQ(kDeferAllScriptFlagLink, defer_all_script_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsDeferAllScriptDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                  kDeferAllScriptFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto defer_all_script_flag = passed_in_flags.find(kDeferAllScriptFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), defer_all_script_flag);
  EXPECT_EQ(flag_descriptions::kEnableDeferAllScriptName,
            defer_all_script_flag->second->description);
  EXPECT_EQ(kDisabledFlagValue, defer_all_script_flag->second->value);
  EXPECT_EQ(kDeferAllScriptFlagLink, defer_all_script_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       GetFlagsLitePageRedirectDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto lite_page_redirect_flag =
      passed_in_flags.find(kLitePageRedirectFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), lite_page_redirect_flag);
  EXPECT_EQ(flag_descriptions::kEnableLitePageServerPreviewsName,
            lite_page_redirect_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, lite_page_redirect_flag->second->value);
  EXPECT_EQ(kLitePageRedirectFlagLink, lite_page_redirect_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsLitePageRedirectEnabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kLitePageRedirectFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto lite_page_redirect_flag =
      passed_in_flags.find(kLitePageRedirectFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), lite_page_redirect_flag);
  EXPECT_EQ(flag_descriptions::kEnableLitePageServerPreviewsName,
            lite_page_redirect_flag->second->description);
  EXPECT_EQ(kEnabledFlagValue, lite_page_redirect_flag->second->value);
  EXPECT_EQ(kLitePageRedirectFlagLink, lite_page_redirect_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsAltConfigCustomValue) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  std::string flag_value = "alt-porg";
  command_line->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyExperiment,
      flag_value);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto alt_config_flag = passed_in_flags.find(kDataSaverAltConfigHtmlId);

  ASSERT_NE(passed_in_flags.end(), alt_config_flag);
  EXPECT_EQ(
      flag_descriptions::kEnableDataReductionProxyServerExperimentDescription,
      alt_config_flag->second->description);
  EXPECT_EQ(flag_value, alt_config_flag->second->value);
  EXPECT_EQ(kDataSaverAltConfigLink, alt_config_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsAltConfigCustomDefault) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto alt_config_flag = passed_in_flags.find(kDataSaverAltConfigHtmlId);

  ASSERT_NE(passed_in_flags.end(), alt_config_flag);
  EXPECT_EQ(
      flag_descriptions::kEnableDataReductionProxyServerExperimentDescription,
      alt_config_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, alt_config_flag->second->value);
  EXPECT_EQ(kDataSaverAltConfigLink, alt_config_flag->second->link);
}

#if defined(OS_ANDROID)
#define TestAndroid(x) x
#else
#define TestAndroid(x) DISABLED_##x
#endif  // OS_ANDROID
TEST_F(InterventionsInternalsPageHandlerTest,
       TestAndroid(GetFlagsOfflinePageDefaultValue)) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto offline_page_flag = passed_in_flags.find(kOfflinePageFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), offline_page_flag);
#if defined(OS_ANDROID)
  EXPECT_EQ(flag_descriptions::kEnableOfflinePreviewsName,
            offline_page_flag->second->description);
#endif  // OS_ANDROID
  EXPECT_EQ(kDefaultFlagValue, offline_page_flag->second->value);
  EXPECT_EQ(kOfflinePageFlagLink, offline_page_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       TestAndroid(GetFlagsOfflinePageEnabled)) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kOfflinePageFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto offline_page_flag = passed_in_flags.find(kOfflinePageFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), offline_page_flag);
#if defined(OS_ANDROID)
  EXPECT_EQ(flag_descriptions::kEnableOfflinePreviewsName,
            offline_page_flag->second->description);
#endif  // OS_ANDROID
  EXPECT_EQ(kEnabledFlagValue, offline_page_flag->second->value);
  EXPECT_EQ(kOfflinePageFlagLink, offline_page_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       TestAndroid(GetFlagsOfflinePageDisabled)) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                  kOfflinePageFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto offline_page_flag = passed_in_flags.find(kOfflinePageFlagHtmlId);

  ASSERT_NE(passed_in_flags.end(), offline_page_flag);
#if defined(OS_ANDROID)
  EXPECT_EQ(flag_descriptions::kEnableOfflinePreviewsName,
            offline_page_flag->second->description);
#endif  // OS_ANDROID
  EXPECT_EQ(kDisabledFlagValue, offline_page_flag->second->value);
  EXPECT_EQ(kOfflinePageFlagLink, offline_page_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, OnNewMessageLogAddedPostToPage) {
  const previews::PreviewsLogger::MessageLog expected_messages[] = {
      previews::PreviewsLogger::MessageLog(
          "Event_a", "Some description a", GURL("http://www.url_a.com/url_a"),
          base::Time::Now(), 1234UL /* page_id */),
      previews::PreviewsLogger::MessageLog(
          "Event_b", "Some description b", GURL("http://www.url_b.com/url_b"),
          base::Time::Now(), 4321UL /* page_id */),
      previews::PreviewsLogger::MessageLog(
          "Event_c", "Some description c", GURL("http://www.url_c.com/url_c"),
          base::Time::Now(), 6789UL /* page_id */),
  };

  for (auto message : expected_messages) {
    page_handler_->OnNewMessageLogAdded(message);
    base::RunLoop().RunUntilIdle();

    mojom::MessageLogPtr* actual = page_->message();
    // Discard any messages generated by network quality tracker.
    while ((*actual)->type == "ECT Changed") {
      page_handler_->OnNewMessageLogAdded(message);
      base::RunLoop().RunUntilIdle();

      actual = page_->message();
    }
    EXPECT_EQ(message.event_type, (*actual)->type);
    EXPECT_EQ(message.event_description, (*actual)->description);
    EXPECT_EQ(message.url, (*actual)->url);
    int64_t expected_time = message.time.ToJavaTime();
    EXPECT_EQ(expected_time, (*actual)->time);
    EXPECT_EQ(message.page_id, (*actual)->page_id);
  }
}

TEST_F(InterventionsInternalsPageHandlerTest, ObserverIsRemovedWhenDestroyed) {
  EXPECT_FALSE(logger_->RemovedObserverIsCalled());
  page_handler_.reset();
  EXPECT_TRUE(logger_->RemovedObserverIsCalled());
}

TEST_F(InterventionsInternalsPageHandlerTest, OnNewBlacklistedHostPostToPage) {
  const std::string hosts[] = {
      "example_0.com", "example_1.com", "example_2.com",
  };

  for (auto expected_host : hosts) {
    base::Time expected_time = base::Time::Now();
    page_handler_->OnNewBlacklistedHost(expected_host, expected_time);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_host, page_->host_blacklisted());
    EXPECT_EQ(expected_time.ToJavaTime(), page_->host_blacklisted_time());
  }
}

TEST_F(InterventionsInternalsPageHandlerTest, OnUserBlacklistedPostToPage) {
  page_handler_->OnUserBlacklistedStatusChange(true /* blacklisted */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(page_->user_blacklisted());

  page_handler_->OnUserBlacklistedStatusChange(false /* blacklisted */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->user_blacklisted());
}

TEST_F(InterventionsInternalsPageHandlerTest, OnBlacklistClearedPostToPage) {
  base::Time times[] = {
      base::Time::FromJsTime(-413696806000),  // Nov 21 1956 20:13:14 UTC
      base::Time::FromJsTime(758620800000),   // Jan 15 1994 08:00:00 UTC
      base::Time::FromJsTime(1581696550000),  // Feb 14 2020 16:09:10 UTC
  };
  for (auto expected_time : times) {
    page_handler_->OnBlacklistCleared(expected_time);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_time.ToJavaTime(), page_->blacklist_cleared_time());
  }
}

TEST_F(InterventionsInternalsPageHandlerTest,
       SetIgnorePreviewsBlacklistDecisionCallsUIServiceCorrectly) {
  page_handler_->SetIgnorePreviewsBlacklistDecision(true /* ignored */);
  EXPECT_TRUE(previews_ui_service_->blacklist_ignored());

  page_handler_->SetIgnorePreviewsBlacklistDecision(false /* ignored */);
  EXPECT_FALSE(previews_ui_service_->blacklist_ignored());
}

TEST_F(InterventionsInternalsPageHandlerTest,
       PageUpdateOnBlacklistIgnoredChange) {
  page_handler_->OnIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(page_->blacklist_ignored());

  page_handler_->OnIgnoreBlacklistDecisionStatusChanged(false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->blacklist_ignored());
}

TEST_F(InterventionsInternalsPageHandlerTest,
       IgnoreBlacklistReversedOnLastObserverRemovedCalled) {
  ASSERT_FALSE(previews::switches::ShouldIgnorePreviewsBlacklist());
  page_handler_->OnLastObserverRemove();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->blacklist_ignored());
}

TEST_F(InterventionsInternalsPageHandlerTest,
       IgnoreBlacklistReversedOnLastObserverRemovedCalledIgnoreViaFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
  ASSERT_TRUE(previews::switches::ShouldIgnorePreviewsBlacklist());
  page_handler_->OnLastObserverRemove();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->blacklist_ignored());
}

}  // namespace
