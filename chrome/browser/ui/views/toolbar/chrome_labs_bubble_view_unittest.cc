// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/test/widget_test.h"

class ChromeLabsBubbleTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeLabs);
    TestWithBrowserView::SetUp();
    ChromeLabsButton* button = chrome_labs_button();
    ChromeLabsBubbleView::Show(button);
  }

  void TearDown() override {
    chrome_labs_bubble()->GetFlagsStateForTesting()->Reset();
    TestWithBrowserView::TearDown();
  }

  ChromeLabsBubbleView* chrome_labs_bubble() {
    return ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting();
  }

  ChromeLabsButton* chrome_labs_button() {
    return browser_view()->toolbar()->chrome_labs_button();
  }

  views::View* chrome_labs_menu_item_container() {
    return ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting()
        ->GetMenuItemContainerForTesting();
  }

  flags_ui::FlagsState* flags_state() {
    return ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting()
        ->GetFlagsStateForTesting();
  }

  flags_ui::FlagsStorage* flags_storage() {
    return ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting()
        ->GetFlagsStorageForTesting();
  }

  ChromeLabsItemView* first_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(menu_items->children().front());
  }

  // Returns true if the option at index |option_index| is the enabled feature
  // state.
  bool IsSelected(int option_index, const flags_ui::FeatureEntry* entry) {
    std::string internal_name = std::string(entry->internal_name) + "@" +
                                base::NumberToString(option_index);
    std::set<std::string> enabled_entries;
    flags_state()->GetSanitizedEnabledFlags(flags_storage(), &enabled_entries);
    for (int i = 0; i < entry->NumOptions(); i++) {
      const std::string name = entry->NameForOption(i);
      if (internal_name == name && enabled_entries.count(name) > 0) {
        return true;
      }
    }
    return false;
  }

  // Returns true if none of the entry's options have been enabled.
  bool IsDefault(const flags_ui::FeatureEntry* entry) {
    std::set<std::string> enabled_entries;
    flags_state()->GetSanitizedEnabledFlags(flags_storage(), &enabled_entries);
    for (int i = 0; i < entry->NumOptions(); i++) {
      const std::string name = entry->NameForOption(i);
      if (enabled_entries.count(name) > 0) {
        return false;
      }
    }
    return true;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ChromeLabsFeatureTest : public ChromeLabsBubbleTest,
                              public testing::WithParamInterface<int> {
 public:
  ChromeLabsFeatureTest() = default;
};

// This test checks that selecting an option through the combobox on a lab will
// enable the corresponding option on the feature.
TEST_P(ChromeLabsFeatureTest, ChangeSelectedOption) {
  int row = GetParam();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  lab_item_combobox->SetSelectedRow(row);

  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry));
}

// For FeatureEntries of type FEATURE_VALUE, the option at index 1 corresponds
// to "Enabled" and the option at index 2 corresponds to "Disabled".
INSTANTIATE_TEST_SUITE_P(All, ChromeLabsFeatureTest, testing::Values(1, 2));

// This test checks that selecting row 0 will reset the feature to it's Default
// state.
TEST_F(ChromeLabsBubbleTest, ResetToDefault) {
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  // Selects an option and then attempts to reset the lab to Default by
  // selecting 0.
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  lab_item_combobox->SetSelectedRow(1);
  EXPECT_FALSE(IsDefault(feature_entry));
  lab_item_combobox->SetSelectedRow(0);
  EXPECT_TRUE(IsDefault(feature_entry));
}

// This test checks that the restart prompt becomes visible when a lab state is
// changed.
TEST_F(ChromeLabsBubbleTest, RestartPromptShows) {
  ChromeLabsBubbleView* bubble_view = chrome_labs_bubble();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();
  EXPECT_FALSE(bubble_view->IsRestartPromptVisibleForTesting());
  lab_item_combobox->SetSelectedRow(1);
  EXPECT_TRUE(bubble_view->IsRestartPromptVisibleForTesting());
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_view->GetWidget());
  ChromeLabsBubbleView::Hide();
  destroyed_waiter.Wait();
  ChromeLabsBubbleView::Show(chrome_labs_button());
  ChromeLabsBubbleView* bubble_view_after_restart = chrome_labs_bubble();
  EXPECT_TRUE(bubble_view_after_restart->IsRestartPromptVisibleForTesting());
}

// This test checks that the restart prompt does not show when the lab state has
// not changed.
// TODO(elainechien): This currently only works for default. This will be
// changed to work for all states. See design doc in crbug/1145666.
TEST_F(ChromeLabsBubbleTest, SelectDefaultTwiceNoRestart) {
  ChromeLabsBubbleView* bubble_view = chrome_labs_bubble();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();
  // Select default state when the originally instantiated state was already
  // default.
  lab_item_combobox->SetSelectedRow(0);
  EXPECT_FALSE(bubble_view->IsRestartPromptVisibleForTesting());
}

// This test checks that IsFeatureSupportedOnPlatform correctly returns whether
// a feature is supported on a platform.
TEST_F(ChromeLabsBubbleTest, IsFeatureSupportedOnPlatformTest) {
  const base::Feature kTestFeature1{"FeatureName1",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
  int os_other_than_current = 1;
  while (os_other_than_current == flags_ui::FlagsState::GetCurrentPlatform())
    os_other_than_current <<= 1;
  flags_ui::FeatureEntry feature_entry_unsupported{
      "flag1", "", "", os_other_than_current,
      FEATURE_VALUE_TYPE(kTestFeature1)};
  EXPECT_FALSE(ChromeLabsBubbleView::IsFeatureSupportedOnPlatform(
      &feature_entry_unsupported));

  const base::Feature kTestFeature2{"FeatureName2",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
  flags_ui::FeatureEntry feature_entry_supported{
      "flag2", "", "", flags_ui::FlagsState::GetCurrentPlatform(),
      FEATURE_VALUE_TYPE(kTestFeature2)};
  EXPECT_TRUE(ChromeLabsBubbleView::IsFeatureSupportedOnPlatform(
      &feature_entry_supported));
}
