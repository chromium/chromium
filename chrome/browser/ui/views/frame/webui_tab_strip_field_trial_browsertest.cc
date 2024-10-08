// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_field_trial.h"

#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/ranges/algorithm.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

void RegisterFakeFieldTrialWithState(base::FeatureList* feature_list,
                                     bool enabled) {
  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial("WebUITabStrip", "Active");
  EXPECT_EQ(field_trial->group_name(), "Active");

  base::FeatureList::OverrideState override_state =
      enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
              : base::FeatureList::OVERRIDE_DISABLE_FEATURE;

  feature_list->RegisterFieldTrialOverride(features::kWebUITabStrip.name,
                                           override_state, field_trial);
  EXPECT_TRUE(feature_list->IsFeatureOverridden(features::kWebUITabStrip.name));
  EXPECT_FALSE(feature_list->IsFeatureOverriddenFromCommandLine(
      features::kWebUITabStrip.name));
}

bool IsInGroup(std::string_view group_name) {
  variations::ActiveGroupId id =
      variations::MakeActiveGroupId("WebUITabStripOnTablets", group_name);

  std::vector<variations::ActiveGroupId> active_groups =
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
          ->GetActiveGroupIds();

  for (const auto& group_id : active_groups) {
    LOG(ERROR) << group_id.name << " " << group_id.group;
  }

  return base::ranges::any_of(active_groups,
                              [=](const variations::ActiveGroupId& e) {
                                return e.name == id.name && e.group == id.group;
                              });
}

}  // namespace

class WebUITabStripFieldTrialBrowserTest : public InProcessBrowserTest {
 public:
  WebUITabStripFieldTrialBrowserTest() {
    variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
        ->ResetForTesting();
    null_feature_list_.InitWithNullFeatureAndFieldTrialLists();
    field_trial_list_ = std::make_unique<base::FieldTrialList>();
  }

 protected:
  void InitFeatureList() {
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list_));
  }

  base::FeatureList* feature_list() { return feature_list_.get(); }

 private:
  // |null_feature_list_| is used to initialize FieldTrialLists to be
  // null. This is required to create a new FieldTrialList.
  base::test::ScopedFeatureList null_feature_list_;
  // |field_trial_list_| is a new FieldTrialList with MockEntryProvider(0.0),
  // created for WebUITabStripFieldTrialBrowserTest.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  // |scoped_feature_list_| is used to enable and disable features for
  // WebUITabStripFieldTrialBrowserTest.
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<base::FeatureList> feature_list_ =
      std::make_unique<base::FeatureList>();
};

// Set feature on by field trial, use system tablet mode capability.
class WebUITabStripFieldTrialDefaultTabletModeBrowserTest
    : public WebUITabStripFieldTrialBrowserTest {
 public:
  WebUITabStripFieldTrialDefaultTabletModeBrowserTest() {
    RegisterFakeFieldTrialWithState(feature_list(), true);
    InitFeatureList();
    EXPECT_TRUE(base::FeatureList::IsEnabled(features::kWebUITabStrip));
  }
};

// Verify the synthetic trial is only enrolled if the devices is a
// tablet.
IN_PROC_BROWSER_TEST_F(WebUITabStripFieldTrialDefaultTabletModeBrowserTest,
                       GroupDependsOnTabletMode) {
  if (WebUITabStripFieldTrial::DeviceIsTabletModeCapable()) {
    EXPECT_TRUE(IsInGroup("Enabled"));
    EXPECT_FALSE(IsInGroup("Disabled"));
    EXPECT_FALSE(IsInGroup("Default"));
  } else {
    EXPECT_FALSE(IsInGroup("Enabled"));
    EXPECT_FALSE(IsInGroup("Disabled"));
    EXPECT_FALSE(IsInGroup("Default"));
  }
}

// The following tests depend on ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// Overrides the device's tablet mode capability, forcing it to appear
// as a tablet.
class WebUITabStripFieldTrialWithTabletModeBrowserTest
    : public WebUITabStripFieldTrialBrowserTest {
 public:
  WebUITabStripFieldTrialWithTabletModeBrowserTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableTabletMode);
  }
};

class WebUITabStripFieldTrialEnabledBrowserTest
    : public WebUITabStripFieldTrialWithTabletModeBrowserTest {
 public:
  WebUITabStripFieldTrialEnabledBrowserTest() {
    RegisterFakeFieldTrialWithState(feature_list(), true);
    InitFeatureList();
    EXPECT_TRUE(base::FeatureList::IsEnabled(features::kWebUITabStrip));
  }
};

// Tablets with WebUITabStrip enabled by field trial should be enrolled
// in the synthetic trial.
IN_PROC_BROWSER_TEST_F(WebUITabStripFieldTrialEnabledBrowserTest, Test) {
  EXPECT_TRUE(IsInGroup("Enabled"));
  EXPECT_FALSE(IsInGroup("Disabled"));
  EXPECT_FALSE(IsInGroup("Default"));
}

class WebUITabStripFieldTrialDisabledBrowserTest
    : public WebUITabStripFieldTrialWithTabletModeBrowserTest {
 public:
  WebUITabStripFieldTrialDisabledBrowserTest() {
    RegisterFakeFieldTrialWithState(feature_list(), false);
    InitFeatureList();
    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kWebUITabStrip));
  }
};

// Tablets with WebUITabStrip disabled by field trial should be enrolled
// in the synthetic trial.
IN_PROC_BROWSER_TEST_F(WebUITabStripFieldTrialDisabledBrowserTest, Test) {
  EXPECT_FALSE(IsInGroup("Enabled"));
  EXPECT_TRUE(IsInGroup("Disabled"));
  EXPECT_FALSE(IsInGroup("Default"));
}

class WebUITabStripFieldTrialCommandLineOverrideBrowserTest
    : public WebUITabStripFieldTrialWithTabletModeBrowserTest {
 public:
  WebUITabStripFieldTrialCommandLineOverrideBrowserTest() {
    feature_list()->InitFromCommandLine("WebUITabStrip", "");
    InitFeatureList();
    EXPECT_TRUE(base::FeatureList::IsEnabled(features::kWebUITabStrip));
  }
};

// If WebUITabStrip is enabled by command line (or about:flags) the
// synthetic field trial shouldn't be registered.
IN_PROC_BROWSER_TEST_F(WebUITabStripFieldTrialCommandLineOverrideBrowserTest,
                       Test) {
  EXPECT_FALSE(IsInGroup("Enabled"));
  EXPECT_FALSE(IsInGroup("Disabled"));
  EXPECT_FALSE(IsInGroup("Default"));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
