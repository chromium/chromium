// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/initial_webui_features_switch.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webui_toolbar {

namespace {

size_t CountParsedEnableFeature(const base::CommandLine& cl,
                                std::string_view target_name) {
  size_t count = 0;
  for (std::string_view entry : base::FeatureList::SplitFeatureListString(
           cl.GetSwitchValueASCII(switches::kEnableFeatures))) {
    std::string name, study, group, params;
    if (base::FeatureList::ParseEnableFeatureString(entry, &name, &study,
                                                    &group, &params)) {
      std::string_view normalized = name;
      if (normalized.starts_with('*')) {
        normalized.remove_prefix(1);
      }
      if (normalized == target_name) {
        ++count;
      }
    }
  }
  return count;
}

class InitialWebUIFeaturesSwitchTest : public ::testing::Test {
 protected:
  void InitFeatureList(const base::CommandLine& cl,
                       std::set<std::string>* enabled_overrides,
                       std::set<std::string>* disabled_overrides) {
    scoped_null_.InitWithNullFeatureAndFieldTrialLists();
    field_trial_list_ = std::make_unique<base::FieldTrialList>();
    base::FieldTrialList::CreateFieldTrial("TestStudy", "TestGroup");
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->InitFromCommandLine(
        cl.GetSwitchValueASCII(switches::kEnableFeatures),
        cl.GetSwitchValueASCII(switches::kDisableFeatures));
    scoped_active_.InitWithFeatureList(std::move(feature_list));

    std::string enabled_str, disabled_str;
    base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_str,
                                                          &disabled_str);
    auto to_set = [](std::string_view csv) {
      std::set<std::string> out;
      for (std::string_view item :
           base::FeatureList::SplitFeatureListString(csv)) {
        out.emplace(item);
      }
      return out;
    };
    *enabled_overrides = to_set(enabled_str);
    *disabled_overrides = to_set(disabled_str);
  }

 private:
  base::test::ScopedFeatureList scoped_null_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList scoped_active_;
};

TEST_F(InitialWebUIFeaturesSwitchTest, Enabled) {
  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(switches::kInitialWebUIFeatures, "enabled");
  cl.AppendSwitchASCII(switches::kEnableFeatures,
                       "CustomFeature,"
                       "InitialWebUISurfaceSync:deadline_in_frames/1,"
                       "TopChromeWebUISharedRenderer.MyGroup");
  cl.AppendSwitchASCII(switches::kDisableFeatures,
                       "InitialWebUI<TestStudy,*WebUIHomeButton");

  ExpandInitialWebUIFeaturesSwitch(cl);

  EXPECT_EQ(CountParsedEnableFeature(cl, "InitialWebUISurfaceSync"), 1u);
  EXPECT_EQ(CountParsedEnableFeature(cl, "TopChromeWebUISharedRenderer"), 1u);
  EXPECT_EQ(CountParsedEnableFeature(cl, "WebUIReloadButton"), 1u);

  std::set<std::string> enabled, disabled;
  InitFeatureList(cl, &enabled, &disabled);
  EXPECT_TRUE(enabled.contains("CustomFeature"));
  EXPECT_TRUE(
      enabled.contains("InitialWebUISurfaceSync<StudyInitialWebUISurfaceSync"));
  EXPECT_EQ(base::GetFieldTrialParamValue("StudyInitialWebUISurfaceSync",
                                          "deadline_in_frames"),
            "1");
  EXPECT_TRUE(enabled.contains("TopChromeWebUISharedRenderer"));
  EXPECT_TRUE(enabled.contains("WebUIReloadButton<StudyWebUIReloadButton"));
  EXPECT_EQ(base::GetFieldTrialParamValue("StudyWebUIReloadButton",
                                          "WebUIReloadButtonPrewarmWebUI"),
            "true");
  EXPECT_TRUE(enabled.contains("*WebUIHomeButton"));
  EXPECT_TRUE(disabled.contains("InitialWebUI<TestStudy"));
  EXPECT_FALSE(enabled.contains("WebUIToolbar"));
  EXPECT_FALSE(disabled.contains("WebUIToolbar"));

  // Re-running expansion is idempotent.
  const std::string already_expanded =
      cl.GetSwitchValueASCII(switches::kEnableFeatures);
  ExpandInitialWebUIFeaturesSwitch(cl);
  EXPECT_EQ(cl.GetSwitchValueASCII(switches::kEnableFeatures),
            already_expanded);
}

TEST_F(InitialWebUIFeaturesSwitchTest, Disabled) {
  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(switches::kInitialWebUIFeatures, "disabled");
  cl.AppendSwitchASCII(switches::kDisableFeatures, "ExistingDisabled");
  cl.AppendSwitchASCII(
      switches::kEnableFeatures,
      "InitialWebUI,*WebUIReloadButton,TopChromeWebUISharedRenderer.MyGroup");

  ExpandInitialWebUIFeaturesSwitch(cl);
  EXPECT_EQ(cl.GetSwitchValueASCII(switches::kDisableFeatures).find(':'),
            std::string::npos);

  std::set<std::string> enabled, disabled;
  InitFeatureList(cl, &enabled, &disabled);
  EXPECT_TRUE(disabled.contains("ExistingDisabled"));
  EXPECT_TRUE(disabled.contains("WebUILocationBar"));
  EXPECT_TRUE(disabled.contains("WebUIHomeButton"));
  EXPECT_TRUE(disabled.contains("WebUIToolbar"));
  EXPECT_TRUE(enabled.contains("InitialWebUI"));
  EXPECT_TRUE(enabled.contains("*WebUIReloadButton"));
  EXPECT_TRUE(enabled.contains("TopChromeWebUISharedRenderer"));
  EXPECT_FALSE(disabled.contains("InitialWebUI"));
  EXPECT_FALSE(disabled.contains("WebUIReloadButton"));
  EXPECT_FALSE(disabled.contains("TopChromeWebUISharedRenderer"));

  // Creating --disable-features from scratch produces clean comma formatting.
  base::CommandLine fresh_cl(base::CommandLine::NO_PROGRAM);
  fresh_cl.AppendSwitchASCII(switches::kInitialWebUIFeatures, "disabled");
  ExpandInitialWebUIFeaturesSwitch(fresh_cl);
  const std::string fresh_disabled =
      fresh_cl.GetSwitchValueASCII(switches::kDisableFeatures);
  EXPECT_FALSE(fresh_disabled.empty());
  EXPECT_FALSE(fresh_disabled.starts_with(','));
  EXPECT_EQ(fresh_disabled.find(",,"), std::string::npos);
}

TEST_F(InitialWebUIFeaturesSwitchTest, IgnoredWhenAbsentOrInvalid) {
  const std::optional<std::string_view> values[] = {std::nullopt, "",
                                                    "invalid"};
  for (const auto& value : values) {
    base::CommandLine cl(base::CommandLine::NO_PROGRAM);
    if (value.has_value()) {
      cl.AppendSwitchASCII(switches::kInitialWebUIFeatures, *value);
    }

    ExpandInitialWebUIFeaturesSwitch(cl);

    EXPECT_FALSE(cl.HasSwitch(switches::kEnableFeatures));
    EXPECT_FALSE(cl.HasSwitch(switches::kDisableFeatures));
  }
}

}  // namespace

}  // namespace webui_toolbar
