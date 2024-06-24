// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_cards_manager.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {

namespace {

// Compare the result of the fetched controllers with the expectation.
void ExpectControllersEqual(
    std::string error_message,
    const std::vector<ReadWriteCardController*>& expected_controllers,
    std::vector<base::WeakPtr<ReadWriteCardController>> actual_controllers) {
  ASSERT_EQ(expected_controllers.size(), actual_controllers.size())
      << error_message;

  for (size_t i = 0; i < expected_controllers.size(); ++i) {
    EXPECT_EQ(expected_controllers[i], actual_controllers[i].get())
        << error_message;
  }
}

}  // namespace

class ReadWriteCardsManagerImplTest : public ChromeAshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  ReadWriteCardsManagerImplTest() = default;

  ReadWriteCardsManagerImplTest(const ReadWriteCardsManagerImplTest&) = delete;
  ReadWriteCardsManagerImplTest& operator=(
      const ReadWriteCardsManagerImplTest&) = delete;

  ~ReadWriteCardsManagerImplTest() override = default;

  // ChromeAshTestBase:
  void SetUp() override {
    if (IsMahiEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kMahi,
                                chromeos::features::kMagicBoost,
                                chromeos::features::kOrca,
                                chromeos::features::kFeatureManagementOrca},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kOrca,
                                chromeos::features::kFeatureManagementOrca},
          /*disabled_features=*/{chromeos::features::kMahi,
                                 chromeos::features::kMagicBoost});
    }

    ChromeAshTestBase::SetUp();

    manager_ = std::make_unique<ReadWriteCardsManagerImpl>();
    magic_boost_state_ = std::make_unique<ash::MagicBoostStateAsh>();
  }

  bool IsMahiEnabled() { return GetParam(); }

  void TearDown() override {
    magic_boost_state_.reset();
    manager_.reset();
    ChromeAshTestBase::TearDown();
  }

  void OnGetEditorModeResult(editor_menu::FetchControllersCallback callback,
                             editor_menu::EditorMode editor_mode) {
    manager_->OnGetEditorModeResult(content::ContextMenuParams(),
                                    std::move(callback), editor_mode);
  }

  QuickAnswersControllerImpl* quick_answers_controller() {
    return manager_->quick_answers_controller_.get();
  }
  chromeos::editor_menu::EditorMenuControllerImpl* editor_menu_controller() {
    return manager_->editor_menu_controller_.get();
  }
  chromeos::mahi::MahiMenuController* mahi_menu_controller() {
    return manager_->mahi_menu_controller_.has_value()
               ? &manager_->mahi_menu_controller_.value()
               : nullptr;
  }
  chromeos::MagicBoostCardController* magic_boost_card_controller() {
    return manager_->magic_boost_card_controller_.has_value()
               ? &manager_->magic_boost_card_controller_.value()
               : nullptr;
  }

 protected:
  std::unique_ptr<ash::MagicBoostStateAsh> magic_boost_state_;
  std::unique_ptr<ReadWriteCardsManagerImpl> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Providing a mock MahiMediaAppEvnetsProxy to satisfy MahiMenuController.
  testing::NiceMock<::ash::MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};
#endif
};

INSTANTIATE_TEST_SUITE_P(, ReadWriteCardsManagerImplTest, testing::Bool());

TEST_P(ReadWriteCardsManagerImplTest, InputPassword) {
  content::ContextMenuParams params;
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;
  TestingProfile profile;

  manager_->FetchController(
      params, &profile,
      base::BindOnce(&ExpectControllersEqual,
                     "No controller should be fetched for password input",
                     std::vector<ReadWriteCardController*>{}));
}

TEST_P(ReadWriteCardsManagerImplTest, QuickAnswersAndMahiControllersApproved) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  TestingProfile profile;

  // Test these behaviors when Magic Boost consent is approved.
  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);
  content::ContextMenuParams params;

  if (IsMahiEnabled()) {
    // When Mahi is enabled and no text is selected, Mahi controller should be
    // fetched.
    manager_->FetchController(
        params, &profile,
        base::BindOnce(
            &ExpectControllersEqual,
            "Wrong quick answers/mahi controller is fetched when no text is "
            "selected and Mahi is enabled",
            std::vector<ReadWriteCardController*>{mahi_menu_controller()}));

    // When Mahi is enabled and text is selected, both Mahi and quick answers
    // controller should be fetched.
    params.selection_text = u"text";
    manager_->FetchController(
        params, &profile,
        base::BindOnce(
            &ExpectControllersEqual,
            "Wrong quick answers/mahi controller is fetched when text is "
            "selected and Mahi is enabled",
            std::vector<ReadWriteCardController*>{quick_answers_controller(),
                                                  mahi_menu_controller()}));
    return;
  }

  // When Mahi is disabled and no text is selected, no controller should be
  // fetched.
  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong quick answers/mahi controller is fetched when no text is "
          "selected and Mahi is disabled",
          std::vector<ReadWriteCardController*>{}));

  // When Mahi is disabled and text is selected, quick answers controller
  // should be fetched.
  params.selection_text = u"text";
  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong quick answers/mahi controller is fetched when text is "
          "selected and Mahi is disabled",
          std::vector<ReadWriteCardController*>{quick_answers_controller()}));
}

TEST_P(ReadWriteCardsManagerImplTest, QuickAnswersAndMahiControllersDeclined) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  TestingProfile profile;

  // Test these behaviors when Magic Boost consent is declined.
  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kDeclined);
  content::ContextMenuParams params;

  if (IsMahiEnabled()) {
    // When Mahi is enabled and no text is selected, Mahi controller should be
    // fetched.
    manager_->FetchController(
        params, &profile,
        base::BindOnce(
            &ExpectControllersEqual,
            "Wrong quick answers/mahi controller is fetched when no text is "
            "selected and Mahi is enabled",
            std::vector<ReadWriteCardController*>{mahi_menu_controller()}));

    // When Mahi is enabled and text is selected, both Mahi and quick answers
    // controller should be fetched.
    params.selection_text = u"text";
    manager_->FetchController(
        params, &profile,
        base::BindOnce(
            &ExpectControllersEqual,
            "Wrong quick answers/mahi controller is fetched when text is "
            "selected and Mahi is enabled",
            std::vector<ReadWriteCardController*>{quick_answers_controller(),
                                                  mahi_menu_controller()}));
    return;
  }

  // When Mahi is disabled and no text is selected, no controller should be
  // fetched.
  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong quick answers/mahi controller is fetched when no text is "
          "selected and Mahi is disabled",
          std::vector<ReadWriteCardController*>{}));

  // When Mahi is disabled and text is selected, quick answers controller
  // should be fetched.
  params.selection_text = u"text";
  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong quick answers/mahi controller is fetched when text is "
          "selected and Mahi is disabled",
          std::vector<ReadWriteCardController*>{quick_answers_controller()}));
}

TEST_P(ReadWriteCardsManagerImplTest,
       MagicBoostOptInQuickAnswerAndMahiNoSelectedText) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kUnset);

  TestingProfile profile;
  content::ContextMenuParams params;

  // When Mahi is enabled and consent status is unset, the opt in card
  // controller should be fetched.
  if (IsMahiEnabled()) {
    manager_->FetchController(
        params, &profile,
        base::BindOnce(
            &ExpectControllersEqual,
            "Wrong quick answers/mahi controller is fetched when "
            "consent status is unset on unselected text and Mahi is enabled",
            std::vector<ReadWriteCardController*>{
                magic_boost_card_controller()}));
    return;
  }

  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong quick answers/mahi controller is fetched when consent status "
          "is unset on unselected text and Mahi is disabled",
          std::vector<ReadWriteCardController*>{}));
}

TEST_P(ReadWriteCardsManagerImplTest,
       MagicBoostOptInQuickAnswerAndMahiSelectedText) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  TestingProfile profile;
  content::ContextMenuParams params;
  params.selection_text = u"text";

  if (IsMahiEnabled()) {
    manager_->FetchController(
        params, &profile,
        base::BindOnce(
            &ExpectControllersEqual,
            "Wrong quick answers/mahi controller is fetched when "
            "consent status is unset on selected text and Mahi is enabled",
            std::vector<ReadWriteCardController*>{
                magic_boost_card_controller()}));
    return;
  }

  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong quick answers/mahi controller is fetched when consent status "
          "is unset on selected text and Mahi is disabled",
          std::vector<ReadWriteCardController*>{quick_answers_controller()}));
}

// Tests that the appropriate controller is returned given the editor mode
// provided in each case.
TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorModeResultBlocked) {
  // If no text is selected and editor mode is kBlocked:
  // - If Mahi is enabled, Mahi is showing opt-in flow (i.e., magic boost card
  //   controller is fetched).
  // - If Mahi is disabled, no card is shown
  OnGetEditorModeResult(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kBlocked",
          IsMahiEnabled()
              ? std::vector<
                    ReadWriteCardController*>{magic_boost_card_controller()}
              : std::vector<ReadWriteCardController*>{}),
      editor_menu::EditorMode::kBlocked);
}

TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorModeResultPromoCard) {
  OnGetEditorModeResult(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kPromoCard",
          IsMahiEnabled()
              ? std::vector<
                    ReadWriteCardController*>{magic_boost_card_controller()}
              : std::vector<
                    ReadWriteCardController*>{editor_menu_controller()}),
      editor_menu::EditorMode::kPromoCard);
}

TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorModeResultWrite) {
  OnGetEditorModeResult(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kWrite",
          std::vector<ReadWriteCardController*>{editor_menu_controller()}),
      editor_menu::EditorMode::kWrite);
}

TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorModeResultRewrite) {
  OnGetEditorModeResult(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kRewrite",
          std::vector<ReadWriteCardController*>{editor_menu_controller()}),
      editor_menu::EditorMode::kRewrite);
}

}  // namespace chromeos
