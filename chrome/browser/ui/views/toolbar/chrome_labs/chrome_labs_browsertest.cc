// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_item_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_view_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/unexpire_flags.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/views/new_badge_label.h"
#include "components/version_info/channel.h"
#include "components/webui/flags/feature_entry_macros.h"
#include "components/webui/flags/flags_state.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
const char kFirstTestFeatureId[] = "feature-1";
BASE_FEATURE(kTestFeature1, "FeatureName1", base::FEATURE_ENABLED_BY_DEFAULT);

// Helper class for setting up Chrome Labs in browser tests. This class
// handles the necessary setup for the Chrome Labs feature to be active and
// provides methods for interacting with its UI.
class ChromeLabsTestHelper {
 public:
  explicit ChromeLabsTestHelper(std::vector<LabInfo> feature_info)
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        // If the channel name is empty on branded builds, STABLE is returned.
        // Force the channel to be a non-stable channel otherwise Chrome Labs
        // will not be shown.
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kFirstTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeature1)}}) {
    scoped_chrome_labs_model_data_.SetModelDataForTesting(
        std::move(feature_info));
    ForceChromeLabsActivationForTesting();
  }

  // Pins the Chrome Labs button to the toolbar. Must be called from
  // SetUpOnMainThread().
  void PinChromeLabsButton(Browser* browser) {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser->GetProfile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, true);
    CHECK(!features::IsWebUIPinnedToolbarActionsEnabled())
        << "Test needs modification to support WebUIPinnedToolbarActions";
    views::test::WaitForAnimatingLayoutManager(
        static_cast<PinnedToolbarActionsContainer*>(
            BrowserView::GetBrowserViewForBrowser(browser)
                ->toolbar_button_provider()
                ->GetPinnedToolbarActions()));
  }

  // Clicks the Chrome Labs button to show the bubble.
  void ShowChromeLabsBubble(Browser* browser) {
    views::Button* chrome_labs_button =
        BrowserView::GetBrowserViewForBrowser(browser)
            ->toolbar()
            ->GetChromeLabsButton();
    views::test::ButtonTestApi(chrome_labs_button).NotifyDefaultMouseClick();
    EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
        kToolbarChromeLabsBubbleElementId));
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

}  // namespace

class ChromeLabsBrowserTest : public InProcessBrowserTest {
 public:
  ChromeLabsBrowserTest() {
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description", "",
         version_info::Channel::STABLE}};
    helper_ =
        std::make_unique<ChromeLabsTestHelper>(std::move(test_feature_info));
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    helper_->PinChromeLabsButton(browser());
  }

  void ShowBubble() { helper_->ShowChromeLabsBubble(browser()); }

 private:
  std::unique_ptr<ChromeLabsTestHelper> helper_;
};

// Asserts the browser process does not crash if the browser window is closed
// while the labs bubble is open.
IN_PROC_BROWSER_TEST_F(ChromeLabsBrowserTest, ClosesWithoutCrashing) {
  ShowBubble();
  CloseBrowserSynchronously(browser());
}

class ChromeLabsUiTest : public DialogBrowserTest {
 public:
  ChromeLabsUiTest() {
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description", "",
         version_info::Channel::STABLE}};
    helper_ =
        std::make_unique<ChromeLabsTestHelper>(std::move(test_feature_info));
  }

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    helper_->PinChromeLabsButton(browser());
  }
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/41419544
    set_should_verify_dialog_bounds(false);
    helper_->ShowChromeLabsBubble(browser());
  }

 private:
  std::unique_ptr<ChromeLabsTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsUiTest, InvokeUi_default) {
  set_baseline("2810222");
  ShowAndVerifyUi();
}

class ChromeLabsMultipleFeaturesUiTest : public DialogBrowserTest {
 public:
  ChromeLabsMultipleFeaturesUiTest() {
    // Add a lot of features to trigger the scrolling functionality.
    // All the entries are linked to the same feature using kFirstTestFeatureId
    // since it doesn't matter what feature is linked.
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description 1", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 2", u"Feature description 2", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 3", u"Feature description 3", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 4", u"Feature description 4", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 5", u"Feature description 5", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 6", u"Feature description 6", "",
         version_info::Channel::STABLE},
    };
    helper_ =
        std::make_unique<ChromeLabsTestHelper>(std::move(test_feature_info));
  }

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    helper_->PinChromeLabsButton(browser());
  }
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/41419544
    set_should_verify_dialog_bounds(false);
    helper_->ShowChromeLabsBubble(browser());

    // Scroll to a little after the dialog inset to ensure that scrolling does
    // not make the contents too close to the title.
    ChromeLabsCoordinator::From(browser())
        ->GetChromeLabsBubbleView()
        ->GetScrollViewForTesting()
        ->ScrollByOffset(
            gfx::PointF(0, views::LayoutProvider::Get()
                                   ->GetInsetsMetric(views::INSETS_DIALOG)
                                   .top() +
                               2));
  }

 private:
  std::unique_ptr<ChromeLabsTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsMultipleFeaturesUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

namespace {

const char kTestFeatureWithVariationId[] = "feature-2";
const char kThirdTestFeatureId[] = "feature-3";
const char kExpiredFlagTestFeatureId[] = "expired-feature";

BASE_FEATURE(kTestFeature2, "FeatureName2", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature3, "FeatureName3", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExpiredFlagTestFeature,
             "Expired",
             base::FEATURE_DISABLED_BY_DEFAULT);

const flags_ui::FeatureEntry::FeatureParam kTestVariationOther2[] = {
    {"Param1", "Value"}};
const flags_ui::FeatureEntry::FeatureVariation kTestVariations2[] = {
    {"Description", kTestVariationOther2, nullptr}};

std::vector<LabInfo> TestLabInfo() {
  std::vector<LabInfo> test_feature_info;
  test_feature_info.emplace_back(kFirstTestFeatureId, u"", u"", "",
                                 version_info::Channel::STABLE);

  std::vector<std::u16string> variation_descriptions = {u"Description"};

  test_feature_info.emplace_back(kTestFeatureWithVariationId, u"", u"", "",
                                 version_info::Channel::STABLE,
                                 variation_descriptions);

  test_feature_info.emplace_back(kThirdTestFeatureId, u"", u"", "",
                                 version_info::Channel::STABLE);

  test_feature_info.emplace_back(kExpiredFlagTestFeatureId, u"", u"", "",
                                 version_info::Channel::STABLE);

  return test_feature_info;
}

base::Time g_mock_time;
base::Time MockTimeNow() {
  return g_mock_time;
}

}  // namespace

class ChromeLabsCoordinatorBrowserTest : public InProcessBrowserTest {
 public:
  ChromeLabsCoordinatorBrowserTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_(
            {{kFirstTestFeatureId, "", "",
              flags_ui::FlagsState::GetCurrentPlatform(),
              FEATURE_VALUE_TYPE(kTestFeature1)},
             {kTestFeatureWithVariationId, "", "",
              flags_ui::FlagsState::GetCurrentPlatform(),
              FEATURE_WITH_PARAMS_VALUE_TYPE(kTestFeature2,
                                             kTestVariations2,
                                             "TestTrial")},
             // kThirdTestFeatureId will be the Id of a FeatureEntry that is
             // not compatible with the current platform.
             {kThirdTestFeatureId, "", "", 0,
              FEATURE_VALUE_TYPE(kTestFeature3)},
             {kExpiredFlagTestFeatureId, "", "",
              flags_ui::FlagsState::GetCurrentPlatform(),
              FEATURE_VALUE_TYPE(kExpiredFlagTestFeature)}}) {
    flags::testing::SetFlagExpiration(kExpiredFlagTestFeatureId, 0);
    scoped_chrome_labs_model_data_.SetModelDataForTesting(TestLabInfo());
    ForceChromeLabsActivationForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->GetProfile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, true);
    if (!features::IsWebUIPinnedToolbarActionsEnabled()) {
      views::test::WaitForAnimatingLayoutManager(
          static_cast<PinnedToolbarActionsContainer*>(
              BrowserView::GetBrowserViewForBrowser(browser())
                  ->toolbar_button_provider()
                  ->GetPinnedToolbarActions()));
    }
    chrome_labs_coordinator_ = ChromeLabsCoordinator::From(browser());
  }

  void TearDownOnMainThread() override {
    if (chrome_labs_coordinator_ && chrome_labs_coordinator_->BubbleExists()) {
      chrome_labs_coordinator_->Hide();
    }
    about_flags::GetCurrentFlagsState()->Reset();
    chrome_labs_coordinator_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  views::View* chrome_labs_menu_item_container() {
    return chrome_labs_coordinator_->GetChromeLabsBubbleView()
        ->GetMenuItemContainerForTesting();
  }

  ChromeLabsItemView* first_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(
        menu_items->children().front().get());
  }

 protected:
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
  raw_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_ = nullptr;

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsCoordinatorBrowserTest, ShowBubbleTest) {
  chrome_labs_coordinator_->Show();
  EXPECT_TRUE(chrome_labs_coordinator_->BubbleExists());

  views::test::WidgetDestroyedWaiter first_destroyed_waiter(
      chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget());
  chrome_labs_coordinator_->Hide();
  first_destroyed_waiter.Wait();
  EXPECT_FALSE(chrome_labs_coordinator_->BubbleExists());
  chrome_labs_coordinator_->Show();
  // The bubble can be closed by the user clicking off of the bubble.
  views::test::WidgetDestroyedWaiter second_destroyed_waiter(
      chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget());
  chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget()->Close();
  second_destroyed_waiter.Wait();
  EXPECT_FALSE(chrome_labs_coordinator_->BubbleExists());
}

// This test checks the new badge shows and that after 8 days the new badge is
// not showing anymore.
IN_PROC_BROWSER_TEST_F(ChromeLabsCoordinatorBrowserTest, NewBadgeTest) {
  g_mock_time = base::Time::Now();
  base::subtle::ScopedTimeClockOverrides time_overrides(&MockTimeNow, nullptr,
                                                        nullptr);
  chrome_labs_coordinator_->Show();
  EXPECT_TRUE(first_lab_item()->GetNewBadgeForTesting()->GetDisplayNewBadge());
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget());
  chrome_labs_coordinator_->Hide();
  destroyed_waiter.Wait();
  constexpr base::TimeDelta kDelay = base::Days(8);
  g_mock_time += kDelay;
  chrome_labs_coordinator_->Show();
  EXPECT_FALSE(first_lab_item()->GetNewBadgeForTesting()->GetDisplayNewBadge());
}

#if BUILDFLAG(IS_CHROMEOS)

// OwnerFlagsStorage on build bots works the same way as the non-owner version
// since we don't have the session manager daemon to write and sign the proto
// blob. This test just opens and closes the bubble to make sure there are no
// crashes.
IN_PROC_BROWSER_TEST_F(ChromeLabsCoordinatorBrowserTest,
                       ShowBubbleWhenUserIsOwner) {
  chrome_labs_coordinator_->Show(
      ChromeLabsCoordinator::ShowUserType::kChromeOsOwnerUserType);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget());
  chrome_labs_coordinator_->Hide();
  destroyed_waiter.Wait();
  chrome_labs_coordinator_->Show(
      ChromeLabsCoordinator::ShowUserType::kChromeOsOwnerUserType);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

// Tests ChromeLabsViewController interactions in a browser test environment.
class ChromeLabsViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  ChromeLabsViewControllerBrowserTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_(
            {{kFirstTestFeatureId, "", "",
              flags_ui::FlagsState::GetCurrentPlatform(),
              FEATURE_VALUE_TYPE(kTestFeature1)},
             {kTestFeatureWithVariationId, "", "",
              flags_ui::FlagsState::GetCurrentPlatform(),
              FEATURE_WITH_PARAMS_VALUE_TYPE(kTestFeature2,
                                             kTestVariations2,
                                             "TestTrial")},
             // kThirdTestFeatureId will be the Id of a FeatureEntry that is
             // not compatible with the current platform.
             {kThirdTestFeatureId, "", "", 0,
              FEATURE_VALUE_TYPE(kTestFeature3)},
             {kExpiredFlagTestFeatureId, "", "",
              flags_ui::FlagsState::GetCurrentPlatform(),
              FEATURE_VALUE_TYPE(kExpiredFlagTestFeature)}}) {
    // Set expiration milestone such that the flag is expired.
    flags::testing::SetFlagExpiration(kExpiredFlagTestFeatureId, 0);
    scoped_chrome_labs_model_data_.SetModelDataForTesting(TestLabInfo());
    ForceChromeLabsActivationForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);

#if BUILDFLAG(IS_CHROMEOS)
    // On ash-chrome we expect the PrefService from the profile to be used.
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        browser()->GetProfile()->GetPrefs());
#else  // !BUILDFLAG(IS_CHROMEOS)
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        g_browser_process->local_state());
#endif

    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->GetProfile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, true);
    if (!features::IsWebUIPinnedToolbarActionsEnabled()) {
      views::test::WaitForAnimatingLayoutManager(
          static_cast<PinnedToolbarActionsContainer*>(
              BrowserView::GetBrowserViewForBrowser(browser())
                  ->toolbar_button_provider()
                  ->GetPinnedToolbarActions()));
    }
    browser()
        ->GetFeatures()
        .pinned_toolbar_actions()
        ->ShowActionEphemerallyInToolbar(kActionShowChromeLabs, true);

    std::unique_ptr<ChromeLabsBubbleView> bubble_view =
        std::make_unique<ChromeLabsBubbleView>(GetChromeLabsButton(),
                                               browser());
    bubble_view_ = bubble_view.get();
    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
    UpdateChromeLabsNewBadgePrefs(browser()->GetProfile());
  }

  void TearDownOnMainThread() override {
    bubble_view_ = nullptr;
    if (bubble_widget_) {
      bubble_widget_.ExtractAsDangling()->CloseNow();
    }
    about_flags::GetCurrentFlagsState()->Reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  ChromeLabsBubbleView* chrome_labs_bubble() { return bubble_view_; }

  views::Button* GetChromeLabsButton() {
    return browser()
        ->GetFeatures()
        .pinned_toolbar_actions()
        ->GetChromeLabsButton();
  }

  views::View* chrome_labs_menu_item_container() {
    return chrome_labs_bubble()->GetMenuItemContainerForTesting();
  }

  flags_ui::FlagsState* flags_state() {
    return about_flags::GetCurrentFlagsState();
  }

  ChromeLabsItemView* first_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(
        menu_items->children().front().get());
  }

  // This corresponds with the feature of type FEATURE_WITH_PARAMS_VALUE
  ChromeLabsItemView* second_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(menu_items->children()[1].get());
  }

  // Returns true if the option at index |option_index| is the enabled feature
  // state in the FlagsStorage we expect the entry to be in.
  bool IsSelected(int option_index,
                  const flags_ui::FeatureEntry* entry,
                  flags_ui::FlagsStorage* expected_flags_storage) {
    std::string internal_name = std::string(entry->internal_name) + "@" +
                                base::NumberToString(option_index);
    std::set<std::string> enabled_entries;
    flags_state()->GetSanitizedEnabledFlags(expected_flags_storage,
                                            &enabled_entries);
    for (int i = 0; i < entry->NumOptions(); i++) {
      const std::string name = entry->NameForOption(i);
      if (internal_name == name && enabled_entries.count(name) > 0) {
        return true;
      }
    }
    return false;
  }

  // Returns true if none of the entry's options have been enabled.
  bool IsDefault(const flags_ui::FeatureEntry* entry,
                 flags_ui::FlagsStorage* expected_flags_storage) {
    std::set<std::string> enabled_entries;
    flags_state()->GetSanitizedEnabledFlags(expected_flags_storage,
                                            &enabled_entries);
    for (int i = 0; i < entry->NumOptions(); i++) {
      const std::string name = entry->NameForOption(i);
      if (enabled_entries.count(name) > 0) {
        return false;
      }
    }
    return true;
  }

  std::unique_ptr<ChromeLabsViewController> CreateViewController() {
    std::unique_ptr<ChromeLabsViewController> view_controller =
        std::make_unique<ChromeLabsViewController>(
            chrome_labs_bubble(), browser(), flags_state(),
            flags_storage_.get());
    return view_controller;
  }

  flags_ui::PrefServiceFlagsStorage* GetFlagsStorage() {
    return flags_storage_.get();
  }

 protected:
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
  raw_ptr<ChromeLabsBubbleView> bubble_view_;
  raw_ptr<views::Widget> bubble_widget_;

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  std::unique_ptr<flags_ui::PrefServiceFlagsStorage> flags_storage_;
};

class ChromeLabsFeatureBrowserTest
    : public ChromeLabsViewControllerBrowserTest,
      public testing::WithParamInterface<int> {
 public:
  ChromeLabsFeatureBrowserTest() = default;
};

#if !BUILDFLAG(IS_CHROMEOS)
// This test checks that selecting an option through the combobox on a lab will
// enable the corresponding option on the feature.
IN_PROC_BROWSER_TEST_P(ChromeLabsFeatureBrowserTest, ChangeSelectedOption) {
  int row = GetParam();
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  // FeatureEntry of type FEATURE_VALUE
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(row);
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry, GetFlagsStorage()));

  // FeatureEntry of type FEATURE_WITH_PARAMS_VALUE
  ChromeLabsItemView* lab_item_with_params = second_lab_item();
  views::Combobox* lab_item_with_params_combobox =
      lab_item_with_params->GetLabStateComboboxForTesting();
  views::test::ComboboxTestApi(lab_item_with_params_combobox)
      .PerformActionAt(row);

  const flags_ui::FeatureEntry* feature_entry_with_params =
      lab_item_with_params->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry_with_params, GetFlagsStorage()));
}

// For FeatureEntries of type FEATURE_VALUE, the option at index 1 corresponds
// to "Enabled" and the option at index 2 corresponds to "Disabled". For
// FeatureEntries of type FEATURE_WITH_PARAMS_VALUE, the option at index 1
// corresponds to "Enabled" and the option at index 2 corresponds to the first
// additional parameter.
INSTANTIATE_TEST_SUITE_P(All,
                         ChromeLabsFeatureBrowserTest,
                         testing::Values(1, 2));

// This test checks that selecting row 0 will reset the feature to it's Default
// state.
IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest, ResetToDefault) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  // Selects an option and then attempts to reset the lab to Default by
  // selecting 0.
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(1);
  EXPECT_FALSE(IsDefault(feature_entry, GetFlagsStorage()));
  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(0);
  EXPECT_TRUE(IsDefault(feature_entry, GetFlagsStorage()));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Ash versions of the above tests.
#if BUILDFLAG(IS_CHROMEOS)

namespace ash {

class ChromeLabsAshFeatureBrowserTest : public ChromeLabsFeatureBrowserTest {
 public:
  ChromeLabsAshFeatureBrowserTest() = default;

  void SetUpOnMainThread() override {
    ChromeLabsFeatureBrowserTest::SetUpOnMainThread();
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
  }
};

IN_PROC_BROWSER_TEST_P(ChromeLabsAshFeatureBrowserTest, ChangeSelectedOption) {
  int row = GetParam();
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  // FeatureEntry of type FEATURE_VALUE
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(row);

  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();

  EXPECT_TRUE(IsSelected(row, feature_entry, GetFlagsStorage()));

  // FeatureEntry of type FEATURE_WITH_PARAMS_VALUE
  ChromeLabsItemView* lab_item_with_params = second_lab_item();
  views::Combobox* lab_item_with_params_combobox =
      lab_item_with_params->GetLabStateComboboxForTesting();
  views::test::ComboboxTestApi(lab_item_with_params_combobox)
      .PerformActionAt(row);

  const flags_ui::FeatureEntry* feature_entry_with_params =
      lab_item_with_params->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry_with_params, GetFlagsStorage()));

  // Make sure flags have been set since ChromeOS should apply flags through
  // the session manager.
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (active_user) {
    AccountId user_id = active_user->GetAccountId();
    std::vector<std::string> raw_flags;
    FakeSessionManagerClient* session_manager = FakeSessionManagerClient::Get();
    view_controller->RestartToApplyFlagsForTesting();
    const bool has_user_flags = session_manager->GetFlagsForUser(
        cryptohome::CreateAccountIdentifierFromAccountId(user_id), &raw_flags);
    EXPECT_TRUE(has_user_flags);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeLabsAshFeatureBrowserTest,
                         testing::Values(1, 2));

IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest, ResetToDefault) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  // Selects an option and then attempts to reset the lab to Default by
  // selecting 0.
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(1);

  EXPECT_FALSE(IsDefault(feature_entry, GetFlagsStorage()));
  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(0);
  EXPECT_TRUE(IsDefault(feature_entry, GetFlagsStorage()));
}

}  // namespace ash

#endif  // BUILDFLAG(IS_CHROMEOS)

// This test checks that only the two features that are supported on the current
// platform and do not have expired flags are added to the bubble.
IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest,
                       OnlyCompatibleFeaturesShow) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();
  EXPECT_EQ(2u, chrome_labs_menu_item_container()->children().size());
}

// This test checks that the restart prompt becomes visible when a lab state is
// changed.
IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest,
                       RestartPromptShows) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();
  ChromeLabsBubbleView* bubble_view = chrome_labs_bubble();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();
  EXPECT_FALSE(bubble_view->IsRestartPromptVisibleForTesting());
  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(1);
  EXPECT_TRUE(bubble_view->IsRestartPromptVisibleForTesting());
  // Check that restart information has been propagated to flags state.
  EXPECT_TRUE(about_flags::IsRestartNeededToCommitChanges());
}

// This test checks that the restart prompt does not show when the lab state has
// not changed.
// TODO(elainechien): This currently only works for default. This will be
// changed to work for all states. See design doc in crbug.com/1145666.
IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest,
                       SelectDefaultTwiceNoRestart) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();
  ChromeLabsBubbleView* bubble_view = chrome_labs_bubble();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();
  // Select default state when the originally instantiated state was already
  // default.
  views::test::ComboboxTestApi(lab_item_combobox).PerformActionAt(0);
  EXPECT_FALSE(bubble_view->IsRestartPromptVisibleForTesting());
}

// TODO(b/185480535): Fix the test for WebUIFeedback
IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest,
                       DISABLED_ShowFeedbackPage) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  base::HistogramTester histogram_tester;

  views::MdTextButton* feedback_button =
      first_lab_item()->GetFeedbackButtonForTesting();
  views::test::ButtonTestApi(feedback_button).NotifyDefaultMouseClick();

  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

// This test checks that experiments that are removed from the model will be
// removed from the PrefService when updating new badge prefs.
IN_PROC_BROWSER_TEST_F(ChromeLabsViewControllerBrowserTest,
                       CleanUpNewBadgePrefsTest) {
  UpdateChromeLabsNewBadgePrefs(browser()->GetProfile());
  const base::DictValue& new_badge_prefs =
#if BUILDFLAG(IS_CHROMEOS)
      browser()->GetProfile()->GetPrefs()->GetDict(
          chrome_labs_prefs::kChromeLabsNewBadgeDictAshChrome);
#else
      g_browser_process->local_state()->GetDict(
          chrome_labs_prefs::kChromeLabsNewBadgeDict);
#endif

  EXPECT_TRUE(new_badge_prefs.contains(kFirstTestFeatureId));
  EXPECT_TRUE(new_badge_prefs.contains(kTestFeatureWithVariationId));

  // Remove two experiments.
  std::vector<LabInfo> test_experiments = TestLabInfo();
  std::erase_if(test_experiments, [](const auto& lab) {
    return lab.internal_name == kFirstTestFeatureId;
  });
  std::erase_if(test_experiments, [](const auto& lab) {
    return lab.internal_name == kTestFeatureWithVariationId;
  });

  scoped_chrome_labs_model_data_.SetModelDataForTesting(test_experiments);

  UpdateChromeLabsNewBadgePrefs(browser()->GetProfile());
  EXPECT_FALSE(new_badge_prefs.contains(kFirstTestFeatureId));
  EXPECT_FALSE(new_badge_prefs.contains(kTestFeatureWithVariationId));
}

#endif  // !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
