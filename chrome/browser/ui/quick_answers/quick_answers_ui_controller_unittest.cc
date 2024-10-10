// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"

namespace {

constexpr char kTitle[] = "default_title";
constexpr char kQuery[] = "default_query";
constexpr gfx::Rect kContextMenuBounds = gfx::Rect(80, 140);
constexpr char kSettingsUrlTemplate[] = "chrome://os-settings/%s?settingId=%d";

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

GURL BuildSettingsURL(const std::string& path,
                      chromeos::settings::mojom::Setting setting_id) {
  return GURL(
      base::StringPrintf(kSettingsUrlTemplate, path.c_str(), setting_id));
}

}  // namespace

class QuickAnswersUiControllerTest : public ChromeQuickAnswersTestBase {
 protected:
  QuickAnswersUiControllerTest() = default;
  QuickAnswersUiControllerTest(const QuickAnswersUiControllerTest&) = delete;
  QuickAnswersUiControllerTest& operator=(const QuickAnswersUiControllerTest&) =
      delete;
  ~QuickAnswersUiControllerTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    ui_controller_ = GetQuickAnswersController()->quick_answers_ui_controller();
  }

  void TearDown() override {
    // `FakeQuickAnswersState` and `QuickAnswersUiController` are owned by
    // `QuickAnswersControllerImpl`, which is owned by
    // `ChromeQuickAnswersTestBase`. Reset early to avoid a dangling pointer.
    fake_quick_answers_state_ = nullptr;
    ui_controller_ = nullptr;

    ChromeQuickAnswersTestBase::TearDown();
  }

  QuickAnswersControllerImpl* GetQuickAnswersController() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  bool MaybeShowConsentView() {
    return GetQuickAnswersController()->MaybeShowUserConsent(
        quick_answers::IntentType::kUnknown,
        /*intent_text=*/u"");
  }

  // Currently instantiated QuickAnswersView instance.
  QuickAnswersUiController* ui_controller() { return ui_controller_; }

 protected:
  std::unique_ptr<QuickAnswersControllerImpl> CreateQuickAnswersControllerImpl(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
      override {
    std::unique_ptr<FakeQuickAnswersState> fake_quick_answers_state =
        std::make_unique<FakeQuickAnswersState>();
    fake_quick_answers_state_ = fake_quick_answers_state.get();
    return std::make_unique<QuickAnswersControllerImpl>(
        read_write_cards_ui_controller, std::move(fake_quick_answers_state));
  }

  FakeQuickAnswersState* fake_quick_answers_state() {
    return fake_quick_answers_state_;
  }

  // A helper method to click a button in `QuickAnswersView`. This does a check
  // if a button is inside `QuickAnswersView`. If you forget to specify context
  // menu bounds, it can be outside of `QuickAnswersView`, which is not easy to
  // notice in test code.
  bool ClickButton(views::Button* button) {
    quick_answers::QuickAnswersView* quick_answers_view =
        ui_controller()->quick_answers_view();

    // If `QuickAnswersView` is too small, a button can be outside of its view.
    // We cannot click it if that's the case.
    if (!quick_answers_view->GetBoundsInScreen().Contains(
            button->GetBoundsInScreen())) {
      return false;
    }

    GetEventGenerator()->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
    return true;
  }

 private:
  raw_ptr<QuickAnswersUiController> ui_controller_ = nullptr;
  raw_ptr<FakeQuickAnswersState> fake_quick_answers_state_ = nullptr;
};

TEST_F(QuickAnswersUiControllerTest, TearDownWhileQuickAnswersViewShowing) {
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();

  GetQuickAnswersController()->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);
  ui_controller()->CreateQuickAnswersView(
      GetProfile(), "default_title", "default_query",
      quick_answers::Intent::kDefinition,
      QuickAnswersState::FeatureType::kQuickAnswers,
      /*is_internal=*/false);
  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersUiControllerTest, ShowAndHideConsentView) {
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->OnContextMenuShown(/*profile=*/nullptr);

  auto* quick_answers_controller = GetQuickAnswersController();

  EXPECT_TRUE(MaybeShowConsentView());

  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());

  auto& read_write_cards_ui_controller =
      quick_answers_controller->read_write_cards_ui_controller();
  auto* user_consent_view = ui_controller()->user_consent_view();

  // The user consent view should appears as the Quick Answers view within
  // `ReadWriteCardsUiController`.
  EXPECT_EQ(user_consent_view,
            read_write_cards_ui_controller.GetQuickAnswersUiForTest());

  // Click on "Allow" button to close the consent view.
  views::test::ButtonTestApi(user_consent_view->allow_button_for_test())
      .NotifyClick(ui::test::TestEvent());

  EXPECT_FALSE(ui_controller()->user_consent_view());
}

TEST_F(QuickAnswersUiControllerTest, TearDownWhileConsentViewShowing) {
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->OnContextMenuShown(GetProfile());

  EXPECT_TRUE(MaybeShowConsentView());

  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
}

TEST_F(QuickAnswersUiControllerTest, QuickAnswersViewAccessibleProperties) {
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);
  ui_controller()->CreateQuickAnswersView(
      GetProfile(), "default_title", "default_query",
      quick_answers::Intent::kDefinition,
      QuickAnswersState::FeatureType::kQuickAnswers,
      /*is_internal=*/false);
  quick_answers::QuickAnswersView* quick_answers_view =
      ui_controller()->quick_answers_view();
  ui::AXNodeData data;

  ASSERT_TRUE(quick_answers_view);
  quick_answers_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
}

TEST_F(QuickAnswersUiControllerTest, OpenSettingsQuickAnswers) {
  MockSettingsWindowManager mock_settings_window_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(
      &mock_settings_window_manager);

  EXPECT_CALL(mock_settings_window_manager,
              ShowChromePageForProfile(
                  testing::_,
                  BuildSettingsURL(
                      chromeos::settings::mojom::kSearchSubpagePath,
                      chromeos::settings::mojom::Setting::kQuickAnswersOnOff),
                  testing::_, testing::_));

  CreateAndShowBasicMenu();

  GetQuickAnswersController()->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);
  ui_controller()->GetReadWriteCardsUiController().SetContextMenuBounds(
      kContextMenuBounds);
  ui_controller()->CreateQuickAnswersView(
      GetProfile(), kTitle, kQuery, quick_answers::Intent::kDefinition,
      QuickAnswersState::FeatureType::kQuickAnswers,
      /*is_internal=*/false);
  quick_answers::QuickAnswersView* quick_answers_view =
      ui_controller()->quick_answers_view();
  ASSERT_TRUE(ClickButton(quick_answers_view->GetSettingsButtonForTesting()));
}

TEST_F(QuickAnswersUiControllerTest, OpenSettingsHmr) {
  MockSettingsWindowManager mock_settings_window_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(
      &mock_settings_window_manager);

  EXPECT_CALL(mock_settings_window_manager,
              ShowChromePageForProfile(
                  testing::_,
                  BuildSettingsURL(
                      chromeos::settings::mojom::kSystemPreferencesSectionPath,
                      chromeos::settings::mojom::Setting::kMahiOnOff),
                  testing::_, testing::_));

  fake_quick_answers_state()->OverrideFeatureType(
      QuickAnswersState::FeatureType::kHmr);
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);
  ui_controller()->GetReadWriteCardsUiController().SetContextMenuBounds(
      kContextMenuBounds);
  ui_controller()->CreateQuickAnswersView(
      GetProfile(), kTitle, kQuery, quick_answers::Intent::kDefinition,
      QuickAnswersState::FeatureType::kQuickAnswers,
      /*is_internal=*/false);
  quick_answers::QuickAnswersView* quick_answers_view =
      ui_controller()->quick_answers_view();
  ASSERT_TRUE(ClickButton(quick_answers_view->GetSettingsButtonForTesting()));
}
