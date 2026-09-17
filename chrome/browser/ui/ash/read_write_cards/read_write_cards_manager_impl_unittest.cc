// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_manager_impl.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/ash/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_manager.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gmock/include/gmock/gmock.h"

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

class ReadWriteCardsManagerImplTest : public ChromeAshTestBase {
 public:
  ReadWriteCardsManagerImplTest() = default;

  ReadWriteCardsManagerImplTest(const ReadWriteCardsManagerImplTest&) = delete;
  ReadWriteCardsManagerImplTest& operator=(
      const ReadWriteCardsManagerImplTest&) = delete;

  ~ReadWriteCardsManagerImplTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kMahiRestrictionsOverride);

    ChromeAshTestBase::SetUp();

    CHECK(profile_manager_.SetUp());
    testing_profile_ =
        profile_manager_.CreateTestingProfile(chrome::kInitialProfile);

    // In production a shared instance of MagicBoostController is initialized
    // by ChromeBrowserMainPartsAsh in its PreProfileInit().
    magic_boost_controller_ = std::make_unique<ash::MagicBoostControllerImpl>();

    // `ReadWriteCardsManagerImpl` will initialize `QuickAnswersState`
    // indirectly. `QuickAnswersState` depends on `MagicBoostState`.
    magic_boost_state_ = std::make_unique<ash::MagicBoostState>(
        base::BindRepeating([]() { return static_cast<Profile*>(nullptr); }));
    manager_ = std::make_unique<ReadWriteCardsManagerImpl>(
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->application_locale_storage(),
        TestingBrowserProcess::GetGlobal()->shared_url_loader_factory());
  }

  void TearDown() override {
    manager_.reset();
    magic_boost_state_.reset();
    magic_boost_controller_.reset();
    testing_profile_ = nullptr;
    profile_manager_.DeleteTestingProfile(chrome::kInitialProfile);
    ChromeAshTestBase::TearDown();
  }

  void OnGetEditorMenuCardContext(
      editor_menu::FetchControllersCallback callback,
      const content::ContextMenuParams& context_menu_params,
      editor_menu::EditorMode editor_mode) {
    const editor_menu::EditorMenuCardContext editor_menu_card_context =
        editor_menu::EditorMenuCardContext()
            .set_editor_mode(editor_mode)
            .build();

    manager_->OnGetEditorMenuCardContext(
        context_menu_params, std::move(callback), editor_menu_card_context);
  }

  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> GetControllers(
      content::ContextMenuParams params,
      editor_menu::EditorMode editor_mode = editor_menu::EditorMode::kWrite) {
    const editor_menu::EditorMenuCardContext editor_menu_card_context =
        editor_menu::EditorMenuCardContext()
            .set_editor_mode(editor_mode)
            .build();

    return manager_->GetControllers(params, editor_menu_card_context);
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
  std::unique_ptr<ash::MagicBoostState> magic_boost_state_;
  std::unique_ptr<ReadWriteCardsManagerImpl> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Providing a mock MahiMediaAppEvnetsProxy to satisfy MahiMenuController.
  testing::NiceMock<::ash::MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};

  std::unique_ptr<ash::MagicBoostControllerImpl> magic_boost_controller_;
  raw_ptr<TestingProfile> testing_profile_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

class ReadWriteCardsManagerImplWithAndWithoutMahiTest
    : public ReadWriteCardsManagerImplTest,
      public testing::WithParamInterface<bool> {
 public:
  // ReadWriteCardsManagerImplTest overrides
  void SetUp() override {
    if (IsMahiEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {
              chromeos::features::kFeatureManagementMahi,
              chromeos::features::kFeatureManagementOrca,
          },
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kFeatureManagementOrca},
          /*disabled_features=*/{chromeos::features::kFeatureManagementMahi});
    }
    ReadWriteCardsManagerImplTest::SetUp();
  }

  bool IsMahiEnabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         ReadWriteCardsManagerImplWithAndWithoutMahiTest,
                         testing::Bool());

TEST_P(ReadWriteCardsManagerImplWithAndWithoutMahiTest, InputPassword) {
  content::ContextMenuParams params;
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;
  TestingProfile profile;

  manager_->FetchController(
      params, &profile,
      base::BindOnce(&ExpectControllersEqual,
                     "No controller should be fetched for password input",
                     std::vector<ReadWriteCardController*>{}));
}

TEST_P(ReadWriteCardsManagerImplWithAndWithoutMahiTest, MahiNotDistillable) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);
  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);

  if (IsMahiEnabled()) {
    mahi_menu_controller()->set_is_distillable_for_testing(false);
  }

  content::ContextMenuParams params;

  // Mahi controller should not be fetched when the page is not distillable.
  EXPECT_TRUE(GetControllers(params).empty())
      << "Wrong quick answers/mahi controller is fetched when no text is "
         "selected and Mahi is enabled";

  params.selection_text = u"text";
  ExpectControllersEqual(
      "Wrong quick answers/mahi controller is fetched when text is "
      "selected and Mahi is enabled",
      std::vector<ReadWriteCardController*>{quick_answers_controller()},
      GetControllers(params));
}

TEST_P(ReadWriteCardsManagerImplWithAndWithoutMahiTest,
       QuickAnswersAndMahiControllersApproved) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  // Test these behaviors when Magic Boost consent is approved.
  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);
  content::ContextMenuParams params;

  if (IsMahiEnabled()) {
    mahi_menu_controller()->set_is_distillable_for_testing(true);

    // When Mahi is enabled and no text is selected, Mahi controller should be
    // fetched.
    ExpectControllersEqual(
        "Wrong quick answers/mahi controller is fetched when no text is "
        "selected and Mahi is enabled",
        std::vector<ReadWriteCardController*>{mahi_menu_controller()},
        GetControllers(params));

    // When Mahi is enabled and text is selected, both Mahi and quick answers
    // controller should be fetched.
    params.selection_text = u"text";
    ExpectControllersEqual(
        "Wrong quick answers/mahi controller is fetched when text is "
        "selected and Mahi is enabled",
        std::vector<ReadWriteCardController*>{quick_answers_controller(),
                                              mahi_menu_controller()},
        GetControllers(params));
    return;
  }

  // When Mahi is disabled and no text is selected, no controller should be
  // fetched.
  EXPECT_TRUE(GetControllers(params).empty())
      << "Wrong quick answers/mahi controller is fetched when no text is "
         "selected and Mahi is disabled";

  // When Mahi is disabled and text is selected, quick answers controller
  // should be fetched.
  params.selection_text = u"text";

  ExpectControllersEqual(
      "Wrong quick answers/mahi controller is fetched when text is "
      "selected and Mahi is disabled",
      std::vector<ReadWriteCardController*>{quick_answers_controller()},
      GetControllers(params));
}

TEST_P(ReadWriteCardsManagerImplWithAndWithoutMahiTest,
       QuickAnswersAndMahiControllersDeclined) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  TestingProfile profile;

  // Test these behaviors when Magic Boost consent is declined.
  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kDeclined);
  content::ContextMenuParams params;

  if (IsMahiEnabled()) {
    mahi_menu_controller()->set_is_distillable_for_testing(true);

    // When Mahi is enabled and Magic Boost consent is declined, no controller
    // should be fetched.
    EXPECT_TRUE(GetControllers(params).empty())
        << "Wrong quick answers/mahi controller is fetched when no text is "
           "selected and Mahi is enabled, and consent is declined";

    params.selection_text = u"text";
    EXPECT_TRUE(GetControllers(params).empty())
        << "Wrong quick answers/mahi controller is fetched when text is "
           "selected and Mahi is enabled, and consent is declined";
    return;
  }

  // When Mahi is disabled and no text is selected, no controller should be
  // fetched.
  EXPECT_TRUE(GetControllers(params).empty())
      << "Wrong quick answers/mahi controller is fetched when no text is "
         "selected and Mahi is disabled";

  // When Mahi is disabled and text is selected, quick answers controller
  // should be fetched.
  params.selection_text = u"text";
  ExpectControllersEqual(
      "Wrong quick answers/mahi controller is fetched when text is "
      "selected and Mahi is disabled",
      std::vector<ReadWriteCardController*>{quick_answers_controller()},
      GetControllers(params));
}

// Tests that no card is shown when the editor is blocked and neither Mahi nor
// Quick Answers is eligible (the page is not distillable and no text is
// selected). `kSoftBlocked` and `kHardBlocked` are handled identically by
// `EditorMenuCardContext::text_and_image_mode()`, so only one is exercised
// here.
TEST_P(ReadWriteCardsManagerImplWithAndWithoutMahiTest,
       OnGetEditorContextBlockedShowsNoCard) {
  if (IsMahiEnabled()) {
    // Mahi is the only card that could still be shown here, so mark the page
    // as non-distillable. This is also required because
    // `MahiMenuController::IsFocusedPageDistillable()` otherwise falls back to
    // the global `MahiWebContentsManager`, which unit tests do not set up.
    mahi_menu_controller()->set_is_distillable_for_testing(false);
  }

  content::ContextMenuParams params;
  params.is_editable = true;

  OnGetEditorMenuCardContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kSoftBlocked",
          std::vector<ReadWriteCardController*>{}),
      params, editor_menu::EditorMode::kSoftBlocked);
}

class ReadWriteCardsManagerImplWithMagicBoostRevampTest
    : public ReadWriteCardsManagerImplTest,
      public testing::WithParamInterface<
          std::tuple</*has_text_selection=*/bool,
                     /*is_textfield_editable=*/bool,
                     /*hmr_consent_status=*/HMRConsentStatus,
                     /*editor_mode=*/editor_menu::EditorMode>> {
 public:
  // ReadWriteCardsManagerImplTest overrides
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kFeatureManagementMahi,
                              chromeos::features::kFeatureManagementOrca},
        /*disabled_features=*/{});

    ReadWriteCardsManagerImplTest::SetUp();
  }

  bool GetHasTextSelectionTestValue() { return std::get<0>(GetParam()); }

  bool GetIsTextfieldEditableTestValue() { return std::get<1>(GetParam()); }

  HMRConsentStatus GetHmrConsentStatusTestValue() {
    return std::get<2>(GetParam());
  }

  editor_menu::EditorMode GetEditorModeTestValue() {
    return std::get<3>(GetParam());
  }

  std::vector<ReadWriteCardController*> GetExpectedListOfControllers() {
    if (GetIsTextfieldEditableTestValue() &&
        (GetEditorModeTestValue() == editor_menu::EditorMode::kConsentNeeded ||
         GetEditorModeTestValue() == editor_menu::EditorMode::kWrite ||
         GetEditorModeTestValue() == editor_menu::EditorMode::kRewrite)) {
      return {editor_menu_controller()};
    }

    if (GetHmrConsentStatusTestValue() != HMRConsentStatus::kDeclined) {
      if (GetHasTextSelectionTestValue()) {
        return {quick_answers_controller(), mahi_menu_controller()};
      }
      return {mahi_menu_controller()};
    }

    return {};
  }

  std::string GetTestFailureMessage() {
    std::string editor_mode = [&](editor_menu::EditorMode editor_mode) {
      switch (editor_mode) {
        case editor_menu::EditorMode::kHardBlocked:
          return "hard blocked";
        case editor_menu::EditorMode::kSoftBlocked:
          return "soft blocked";
        case editor_menu::EditorMode::kConsentNeeded:
          return "consent needed";
        case editor_menu::EditorMode::kRewrite:
          return "rewrite";
        case editor_menu::EditorMode::kWrite:
          return "write";
      }
    }(GetEditorModeTestValue());

    std::string hmr_consent_status = [&](HMRConsentStatus consent_status) {
      switch (consent_status) {
        case chromeos::HMRConsentStatus::kUnset:
          return "unset";
        case chromeos::HMRConsentStatus::kApproved:
          return "approved";
        case chromeos::HMRConsentStatus::kDeclined:
          return "declined";
        case chromeos::HMRConsentStatus::kPendingDisclaimer:
          return "pending disclaimer";
      }
    }(GetHmrConsentStatusTestValue());

    return base::StrCat(
        {"Wrong controller is fetched when ",
         GetHasTextSelectionTestValue() ? "some text" : "no text",
         " is selected, the text is ",
         GetIsTextfieldEditableTestValue() ? "editable" : "non-editable",
         ", editor is in ", editor_mode, " mode and hmr consent status is ",
         hmr_consent_status});
  }
};

TEST_P(ReadWriteCardsManagerImplWithMagicBoostRevampTest, GetControllers) {
  mahi_menu_controller()->set_is_distillable_for_testing(true);
  magic_boost_state_->AsyncWriteConsentStatus(GetHmrConsentStatusTestValue());
  QuickAnswersState::Get()->SetEligibilityForTesting(true);
  content::ContextMenuParams context_menu_params;
  context_menu_params.is_editable = GetIsTextfieldEditableTestValue();
  context_menu_params.selection_text =
      GetHasTextSelectionTestValue() ? u"text selection" : u"";

  OnGetEditorMenuCardContext(
      base::BindOnce(&ExpectControllersEqual, GetTestFailureMessage(),
                     GetExpectedListOfControllers()),
      context_menu_params,
      /*editor_mode=*/GetEditorModeTestValue());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ReadWriteCardsManagerImplWithMagicBoostRevampTest,
    testing::Combine(
        /*has_text_selection=*/testing::Bool(),
        /*is_textfield_editable=*/testing::Bool(),
        /*hmr_consent_status=*/
        testing::Values(chromeos::HMRConsentStatus::kUnset,
                        chromeos::HMRConsentStatus::kApproved,
                        chromeos::HMRConsentStatus::kDeclined,
                        chromeos::HMRConsentStatus::kPendingDisclaimer),
        /*editor_mode=*/
        testing::Values(chromeos::editor_menu::EditorMode::kHardBlocked,
                        chromeos::editor_menu::EditorMode::kSoftBlocked,
                        chromeos::editor_menu::EditorMode::kConsentNeeded,
                        chromeos::editor_menu::EditorMode::kRewrite,
                        chromeos::editor_menu::EditorMode::kWrite)));

}  // namespace chromeos
