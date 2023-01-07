// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_starter_heuristic_config.h"
#include <memory>
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

// A correctly formatted launch config.
const char kRegularConfig[] = R"(
  {
    "intent": "SOME_INTENT",
    "denylistedDomains": ["example1.com", "example2.com"],
    "heuristics": [
      {
        "conditionSet": {
          "urlContains":"something"
        }
      }
    ],
    "enabledInCustomTabs":true,
    "enabledInRegularTabs":false,
    "enabledInWeblayer":false,
    "enabledForSignedOutUsers":true,
    "enabledWithoutMsbb":false
  }
  )";

// Sets up the platform delegate to satisfy the conditions in |kRegularConfig|.
void SetupForRegularConfig(FakeStarterPlatformDelegate* fake_delegate) {
  fake_delegate->is_custom_tab_ = true;
  fake_delegate->is_web_layer_ = false;
  fake_delegate->is_tab_created_by_gsa_ = true;
  fake_delegate->is_logged_in_ = false;
  fake_delegate->fake_common_dependencies_->msbb_enabled_ = true;
}

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class LaunchedStarterHeuristicConfigTest : public testing::Test {
 public:
  LaunchedStarterHeuristicConfigTest() = default;
  ~LaunchedStarterHeuristicConfigTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(
          std::make_unique<FakeCommonDependencies>(nullptr));
};

TEST_F(LaunchedStarterHeuristicConfigTest, NoConfigsIfEmptyParameters) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      features::kAutofillAssistantUrlHeuristic1);

  std::string empty_parameters;
  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, empty_parameters, {"us"}};
  SetupForRegularConfig(&fake_platform_delegate_);
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LaunchedStarterHeuristicConfigTest, NoConfigsIfInvalidParameters) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      features::kAutofillAssistantUrlHeuristic1);
  std::string parameters = "not valid json";

  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, parameters, {"us"}};
  SetupForRegularConfig(&fake_platform_delegate_);
  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LaunchedStarterHeuristicConfigTest, RegularConfigWorksIfFeatureEnabled) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      features::kAutofillAssistantUrlHeuristic1);
  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, kRegularConfig, {"us"}};

  SetupForRegularConfig(&fake_platform_delegate_);
  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "us";

  EXPECT_THAT(config.GetDenylistedDomains(),
              UnorderedElementsAre("example1.com", "example2.com"));
  EXPECT_THAT(config.GetIntent(), Eq("SOME_INTENT"));
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));

  // The different combinations of available flags are more extensively tested
  // in |FinchStarterHeuristicConfig|.
  fake_platform_delegate_.is_custom_tab_ = false;
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LaunchedStarterHeuristicConfigTest,
       RegularConfigDoesNotWorkIfFeatureDisabled) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndDisableFeature(
      features::kAutofillAssistantUrlHeuristic1);
  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, kRegularConfig, {"us"}};

  SetupForRegularConfig(&fake_platform_delegate_);
  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "us";

  EXPECT_THAT(config.GetDenylistedDomains(), IsEmpty());
  EXPECT_THAT(config.GetIntent(), IsEmpty());
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());
}

TEST_F(LaunchedStarterHeuristicConfigTest,
       RegularConfigDoesNotWorkIfCountryNotLaunched) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      features::kAutofillAssistantUrlHeuristic1);
  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, kRegularConfig, {"us", "gb"}};

  SetupForRegularConfig(&fake_platform_delegate_);
  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "ch";
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              IsEmpty());

  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "gb";
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(LaunchedStarterHeuristicConfigTest, PreferPermanentCountry) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      features::kAutofillAssistantUrlHeuristic1);
  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, kRegularConfig, {"us"}};

  SetupForRegularConfig(&fake_platform_delegate_);
  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "us";
  fake_platform_delegate_.fake_common_dependencies_->latest_country_code_ =
      "zz";
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

TEST_F(LaunchedStarterHeuristicConfigTest,
       FallbackToLatestCountryIfNoPermanentCountry) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(
      features::kAutofillAssistantUrlHeuristic1);
  LaunchedStarterHeuristicConfig config{
      features::kAutofillAssistantUrlHeuristic1, kRegularConfig, {"us"}};

  SetupForRegularConfig(&fake_platform_delegate_);
  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "zz";
  fake_platform_delegate_.fake_common_dependencies_->latest_country_code_ =
      "us";
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));

  fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
      "";
  EXPECT_THAT(config.GetConditionSetsForClientState(&fake_platform_delegate_,
                                                    &context_),
              SizeIs(1));
}

}  // namespace
}  // namespace autofill_assistant
