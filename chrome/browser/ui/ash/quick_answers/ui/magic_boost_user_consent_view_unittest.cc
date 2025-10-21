// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/magic_boost_user_consent_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/ash/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/test_layout_provider.h"

namespace quick_answers {

namespace {

constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(120, 140));

class MockSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  MOCK_METHOD(void,
              ShowChromePageForProfile,
              (Profile * profile,
               const GURL& gurl,
               int64_t display_id,
               apps::LaunchCallback callback),
              (override));
};

}  // namespace

class MagicBoostUserConsentViewTest : public ChromeQuickAnswersTestBase {
 protected:
  MagicBoostUserConsentViewTest() = default;
  MagicBoostUserConsentViewTest(const MagicBoostUserConsentViewTest&) = delete;
  MagicBoostUserConsentViewTest& operator=(
      const MagicBoostUserConsentViewTest&) = delete;
  ~MagicBoostUserConsentViewTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    feature_list_.InitWithFeatures(
        {chromeos::features::kMagicBoostRevampForQuickAnswers}, {});
  }

  void TearDown() override {
    fake_quick_answers_state_ = nullptr;

    ChromeQuickAnswersTestBase::TearDown();
  }

  std::unique_ptr<QuickAnswersControllerImpl> CreateQuickAnswersControllerImpl(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
      override {
    std::unique_ptr<FakeQuickAnswersState> fake_quick_answers_state =
        std::make_unique<FakeQuickAnswersState>();
    fake_quick_answers_state_ = fake_quick_answers_state.get();
    fake_quick_answers_state_->OverrideFeatureType(
        QuickAnswersState::FeatureType::kHmr);
    return std::make_unique<QuickAnswersControllerImpl>(
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->application_locale_storage(),
        read_write_cards_ui_controller, std::move(fake_quick_answers_state));
  }

 public:
  QuickAnswersControllerImpl* GetQuickAnswersController() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  QuickAnswersUiController* GetUiController() {
    return GetQuickAnswersController()->quick_answers_ui_controller();
  }

  MagicBoostUserConsentView* GetMagicBoostUserConsentView() {
    return views::AsViewClass<MagicBoostUserConsentView>(
        GetUiController()->magic_boost_user_consent_view());
  }

  void CreateUserConsentView() {
    GetQuickAnswersController()->SetVisibility(
        QuickAnswersVisibility::kPending);
    // Set up a companion menu before creating the QuickAnswersView.
    CreateAndShowBasicMenu();
    GetUiController()->GetReadWriteCardsUiController().SetContextMenuBounds(
        kDefaultAnchorBoundsInScreen);

    static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get())
        ->SetVisibility(QuickAnswersVisibility::kPending);
    GetUiController()->CreateUserConsentView(
        GetProfile(), kDefaultAnchorBoundsInScreen,
        quick_answers::IntentType::kUnit, u"Text");
  }

  void SimulateSettingsButtonClicked() {
    views::test::ButtonTestApi(
        GetMagicBoostUserConsentView()->settings_button_for_testing())
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  chromeos::ReadWriteCardsUiController controller_;
  raw_ptr<FakeQuickAnswersState> fake_quick_answers_state_ = nullptr;
};

TEST_F(MagicBoostUserConsentViewTest, TranslateButtonTextLabel) {
  MagicBoostUserConsentView magic_boost_user_consent_view(
      IntentType::kTranslation,
      /*intent_text=*/u"testing label",
      GetUiController()->GetReadWriteCardsUiController());

  EXPECT_EQ(u"Translate \"testing label\"",
            magic_boost_user_consent_view.chip_label_for_testing());
}

TEST_F(MagicBoostUserConsentViewTest, DefineButtonTextLabel) {
  MagicBoostUserConsentView magic_boost_user_consent_view(
      IntentType::kDictionary,
      /*intent_text=*/u"testing label",
      GetUiController()->GetReadWriteCardsUiController());

  EXPECT_EQ(u"Define \"testing label\"",
            magic_boost_user_consent_view.chip_label_for_testing());
}

TEST_F(MagicBoostUserConsentViewTest, ConvertButtonTextLabel) {
  MagicBoostUserConsentView magic_boost_user_consent_view(
      IntentType::kUnit,
      /*intent_text=*/u"testing label",
      GetUiController()->GetReadWriteCardsUiController());

  EXPECT_EQ(u"Convert \"testing label\"",
            magic_boost_user_consent_view.chip_label_for_testing());
}

TEST_F(MagicBoostUserConsentViewTest, OpenSettings) {
  MockSettingsWindowManager mock_settings_window_manager;

  CreateUserConsentView();

  EXPECT_CALL(
      mock_settings_window_manager,
      ShowChromePageForProfile(testing::_, testing::_, testing::_, testing::_));

  SimulateSettingsButtonClicked();
}

TEST_F(MagicBoostUserConsentViewTest, A11yNameAndDescription) {
  MagicBoostUserConsentView magic_boost_user_consent_view(
      IntentType::kUnit,
      /*intent_text=*/u"XYZ",
      GetUiController()->GetReadWriteCardsUiController());

  EXPECT_EQ(u"Convert \"XYZ\"",
            magic_boost_user_consent_view.GetAccessibleName());
  EXPECT_EQ(
      u"Right-click or press and hold to get definitions, translations, or "
      u"unit conversions. Use Left or Right arrow keys to manage this feature.",
      magic_boost_user_consent_view.GetAccessibleDescription());
}

}  // namespace quick_answers
