// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_item_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_view_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/user_education/views/new_badge_label.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

const char kFirstTestFeatureId[] = "feature-1";
const char kTestFeatureWithVariationId[] = "feature-2";
const char kThirdTestFeatureId[] = "feature-3";
const char kExpiredFlagTestFeatureId[] = "expired-feature";

BASE_FEATURE(kTestFeature1, "FeatureName1", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2, "FeatureName2", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature3, "FeatureName3", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExpiredFlagTestFeature,
             "Expired",
             base::FEATURE_DISABLED_BY_DEFAULT);

const flags_ui::FeatureEntry::FeatureParam kTestVariationOther2[] = {
    {"Param1", "Value"}};
const flags_ui::FeatureEntry::FeatureVariation kTestVariations2[] = {
    {"Description", kTestVariationOther2, 1, nullptr}};

std::vector<LabInfo> TestLabInfo() {
  std::vector<LabInfo> test_feature_info;
  test_feature_info.emplace_back(LabInfo(kFirstTestFeatureId, u"", u"", "",
                                         version_info::Channel::STABLE));

  std::vector<std::u16string> variation_descriptions = {u"Description"};

  test_feature_info.emplace_back(LabInfo(kTestFeatureWithVariationId, u"", u"",
                                         "", version_info::Channel::STABLE,
                                         variation_descriptions));

  test_feature_info.emplace_back(LabInfo(kThirdTestFeatureId, u"", u"", "",
                                         version_info::Channel::STABLE));

  test_feature_info.emplace_back(LabInfo(kExpiredFlagTestFeatureId, u"", u"",
                                         "", version_info::Channel::STABLE));

  return test_feature_info;
}

}  // namespace

class ChromeLabsCoordinatorTest : public TestWithBrowserView {
 public:
  ChromeLabsCoordinatorTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
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
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});

    // Set up test data on the model.
    scoped_chrome_labs_model_data_.SetModelDataForTesting(TestLabInfo());

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);

    chrome_labs_coordinator_ =
        std::make_unique<ChromeLabsCoordinator>(browser_view()->browser());
  }

  void TearDown() override {
    about_flags::GetCurrentFlagsState()->Reset();
    TestWithBrowserView::TearDown();
  }

  views::View* chrome_labs_menu_item_container() {
    return chrome_labs_coordinator_->GetChromeLabsBubbleView()
        ->GetMenuItemContainerForTesting();
  }

  ChromeLabsModel* chrome_labs_model() {
    return browser_view()->toolbar()->chrome_labs_model();
  }

  ChromeLabsItemView* first_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(menu_items->children().front());
  }

 protected:
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
  std::unique_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_;

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeLabsCoordinatorTest, ShowBubbleTest) {
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
TEST_F(ChromeLabsCoordinatorTest, NewBadgeTest) {
  chrome_labs_coordinator_->Show();
  EXPECT_TRUE(first_lab_item()->GetNewBadgeForTesting()->GetDisplayNewBadge());
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget());
  chrome_labs_coordinator_->Hide();
  destroyed_waiter.Wait();
  constexpr base::TimeDelta kDelay = base::Days(8);
  task_environment()->AdvanceClock(kDelay);
  chrome_labs_coordinator_->Show();
  EXPECT_FALSE(first_lab_item()->GetNewBadgeForTesting()->GetDisplayNewBadge());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// OwnerFlagsStorage on build bots works the same way as the non-owner version
// since we don't have the session manager daemon to write and sign the proto
// blob. This test just opens and closes the bubble to make sure there are no
// crashes.
TEST_F(ChromeLabsCoordinatorTest, ShowBubbleWhenUserIsOwner) {
  chrome_labs_coordinator_->Show(
      ChromeLabsCoordinator::ShowUserType::kChromeOsOwnerUserType);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      chrome_labs_coordinator_->GetChromeLabsBubbleView()->GetWidget());
  chrome_labs_coordinator_->Hide();
  destroyed_waiter.Wait();
  chrome_labs_coordinator_->Show(
      ChromeLabsCoordinator::ShowUserType::kChromeOsOwnerUserType);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ChromeLabsViewControllerTest : public TestWithBrowserView {
 public:
  ChromeLabsViewControllerTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
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
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});

    // Set up test data on the model.
    scoped_chrome_labs_model_data_.SetModelDataForTesting(TestLabInfo());
    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On ash-chrome we expect the PrefService from the profile to be used.
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        profile()->GetPrefs());
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        TestingBrowserProcess::GetGlobal()->local_state());
#endif

    std::unique_ptr<ChromeLabsBubbleView> bubble_view =
        std::make_unique<ChromeLabsBubbleView>(GetChromeLabsButton(),
                                               browser());
    bubble_view_ = bubble_view.get();
    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  }

  void TearDown() override {
    about_flags::GetCurrentFlagsState()->Reset();
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    TestWithBrowserView::TearDown();
  }

  ChromeLabsBubbleView* chrome_labs_bubble() { return bubble_view_; }

  views::Button* GetChromeLabsButton() {
    return browser_view()->toolbar()->GetChromeLabsButton();
  }

  views::View* chrome_labs_menu_item_container() {
    return chrome_labs_bubble()->GetMenuItemContainerForTesting();
  }

  ChromeLabsModel* chrome_labs_model() {
    return browser_view()->toolbar()->chrome_labs_model();
  }

  flags_ui::FlagsState* flags_state() {
    return about_flags::GetCurrentFlagsState();
  }

  ChromeLabsItemView* first_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(menu_items->children().front());
  }

  // This corresponds with the feature of type FEATURE_WITH_PARAMS_VALUE
  ChromeLabsItemView* second_lab_item() {
    views::View* menu_items = chrome_labs_menu_item_container();
    return static_cast<ChromeLabsItemView*>(menu_items->children()[1]);
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
            chrome_labs_model(), chrome_labs_bubble(),
            browser_view()->browser(), flags_state(), flags_storage_.get());
    return view_controller;
  }

  flags_ui::PrefServiceFlagsStorage* GetFlagsStorage() {
    return flags_storage_.get();
  }

 protected:
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
  raw_ptr<ChromeLabsBubbleView, DanglingUntriaged> bubble_view_;
  raw_ptr<views::Widget, DanglingUntriaged> bubble_widget_;

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<flags_ui::PrefServiceFlagsStorage> flags_storage_;
};

class ChromeLabsFeatureTest : public ChromeLabsViewControllerTest,
                              public testing::WithParamInterface<int> {
 public:
  ChromeLabsFeatureTest() = default;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// This test checks that selecting an option through the combobox on a lab will
// enable the corresponding option on the feature.
TEST_P(ChromeLabsFeatureTest, ChangeSelectedOption) {
  int row = GetParam();
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  // FeatureEntry of type FEATURE_VALUE
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  lab_item_combobox->SetSelectedRow(row);
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry, GetFlagsStorage()));

  // FeatureEntry of type FEATURE_WITH_PARAMS_VALUE
  ChromeLabsItemView* lab_item_with_params = second_lab_item();
  views::Combobox* lab_item_with_params_combobox =
      lab_item_with_params->GetLabStateComboboxForTesting();
  lab_item_with_params_combobox->SetSelectedRow(row);

  const flags_ui::FeatureEntry* feature_entry_with_params =
      lab_item_with_params->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry_with_params, GetFlagsStorage()));
}

// For FeatureEntries of type FEATURE_VALUE, the option at index 1 corresponds
// to "Enabled" and the option at index 2 corresponds to "Disabled". For
// FeatureEntries of type FEATURE_WITH_PARAMS_VALUE, the option at index 1
// corresponds to "Enabled" and the option at index 2 corresponds to the first
// additional parameter.
INSTANTIATE_TEST_SUITE_P(All, ChromeLabsFeatureTest, testing::Values(1, 2));

// This test checks that selecting row 0 will reset the feature to it's Default
// state.
TEST_F(ChromeLabsViewControllerTest, ResetToDefault) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  // Selects an option and then attempts to reset the lab to Default by
  // selecting 0.
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  lab_item_combobox->SetSelectedRow(1);
  EXPECT_FALSE(IsDefault(feature_entry, GetFlagsStorage()));
  lab_item_combobox->SetSelectedRow(0);
  EXPECT_TRUE(IsDefault(feature_entry, GetFlagsStorage()));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Ash versions of the above tests.
#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace ash {

class ChromeLabsAshFeatureTest : public ChromeLabsFeatureTest {
 public:
  ChromeLabsAshFeatureTest() {
    SessionManagerClient::InitializeFakeInMemory();
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
  }
};

TEST_P(ChromeLabsAshFeatureTest, ChangeSelectedOption) {
  int row = GetParam();
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  // FeatureEntry of type FEATURE_VALUE
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  lab_item_combobox->SetSelectedRow(row);

  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();

  EXPECT_TRUE(IsSelected(row, feature_entry, GetFlagsStorage()));

  // FeatureEntry of type FEATURE_WITH_PARAMS_VALUE
  ChromeLabsItemView* lab_item_with_params = second_lab_item();
  views::Combobox* lab_item_with_params_combobox =
      lab_item_with_params->GetLabStateComboboxForTesting();
  lab_item_with_params_combobox->SetSelectedRow(row);

  const flags_ui::FeatureEntry* feature_entry_with_params =
      lab_item_with_params->GetFeatureEntry();
  EXPECT_TRUE(IsSelected(row, feature_entry_with_params, GetFlagsStorage()));

  // Make sure flags have been set since ChromeOS should apply flags through
  // the session manager.
  AccountId user_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  std::vector<std::string> raw_flags;
  FakeSessionManagerClient* session_manager = FakeSessionManagerClient::Get();
  view_controller->RestartToApplyFlagsForTesting();
  const bool has_user_flags = session_manager->GetFlagsForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(user_id), &raw_flags);
  EXPECT_TRUE(has_user_flags);
}

INSTANTIATE_TEST_SUITE_P(All, ChromeLabsAshFeatureTest, testing::Values(1, 2));

TEST_F(ChromeLabsViewControllerTest, ResetToDefault) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();

  // Selects an option and then attempts to reset the lab to Default by
  // selecting 0.
  const flags_ui::FeatureEntry* feature_entry = lab_item->GetFeatureEntry();
  lab_item_combobox->SetSelectedRow(1);

  EXPECT_FALSE(IsDefault(feature_entry, GetFlagsStorage()));
  lab_item_combobox->SetSelectedRow(0);
  EXPECT_TRUE(IsDefault(feature_entry, GetFlagsStorage()));
}

}  // namespace ash

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// This test checks that only the two features that are supported on the current
// platform and do not have expired flags are added to the bubble.
TEST_F(ChromeLabsViewControllerTest, OnlyCompatibleFeaturesShow) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();
  EXPECT_TRUE(chrome_labs_menu_item_container()->children().size() == 2);
}

// This test checks that the restart prompt becomes visible when a lab state is
// changed.
TEST_F(ChromeLabsViewControllerTest, RestartPromptShows) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();
  ChromeLabsBubbleView* bubble_view = chrome_labs_bubble();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();
  EXPECT_FALSE(bubble_view->IsRestartPromptVisibleForTesting());
  lab_item_combobox->SetSelectedRow(1);
  EXPECT_TRUE(bubble_view->IsRestartPromptVisibleForTesting());
  // Check that restart information has been propagated to flags state.
  EXPECT_TRUE(about_flags::IsRestartNeededToCommitChanges());
}

// This test checks that the restart prompt does not show when the lab state has
// not changed.
// TODO(elainechien): This currently only works for default. This will be
// changed to work for all states. See design doc in crbug/1145666.
TEST_F(ChromeLabsViewControllerTest, SelectDefaultTwiceNoRestart) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();
  ChromeLabsBubbleView* bubble_view = chrome_labs_bubble();
  ChromeLabsItemView* lab_item = first_lab_item();
  views::Combobox* lab_item_combobox =
      lab_item->GetLabStateComboboxForTesting();
  // Select default state when the originally instantiated state was already
  // default.
  lab_item_combobox->SetSelectedRow(0);
  EXPECT_FALSE(bubble_view->IsRestartPromptVisibleForTesting());
}

// TODO(crbug.com/40719879)
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(b/185480535): Fix the test for WebUIFeedback
TEST_F(ChromeLabsViewControllerTest, DISABLED_ShowFeedbackPage) {
  std::unique_ptr<ChromeLabsViewController> view_controller =
      CreateViewController();

  base::HistogramTester histogram_tester;

  views::MdTextButton* feedback_button =
      first_lab_item()->GetFeedbackButtonForTesting();
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(feedback_button);
  test_api.NotifyClick(e);

  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}
#endif

// This test checks that experiments that are removed from the model will be
// removed from the PrefService when updating new badge prefs.
TEST_F(ChromeLabsViewControllerTest, CleanUpNewBadgePrefsTest) {
  const base::Value::Dict& new_badge_prefs =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      browser_view()->browser()->profile()->GetPrefs()->GetDict(
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

  UpdateChromeLabsNewBadgePrefs(browser_view()->browser()->profile(),
                                chrome_labs_model());
  EXPECT_FALSE(new_badge_prefs.contains(kFirstTestFeatureId));
  EXPECT_FALSE(new_badge_prefs.contains(kTestFeatureWithVariationId));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
