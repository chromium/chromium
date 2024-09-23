// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager_impl.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
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
                                chromeos::features::kOrca,
                                chromeos::features::kFeatureManagementMahi,
                                chromeos::features::kFeatureManagementOrca},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kOrca,
                                chromeos::features::kFeatureManagementOrca},
          /*disabled_features=*/{chromeos::features::kMahi,
                                 chromeos::features::kFeatureManagementMahi});
    }

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kMahiRestrictionsOverride);

    ChromeAshTestBase::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Creates test Crosapi manger, which depends on `ProfileManger` and
    // `LoginState`. Otherwise there will be a null pointer issue, since
    // `crosapi::CrosapiManager::Get()->crosapi_ash()` is null.
    CHECK(profile_manager_.SetUp());
    testing_profile_ =
        profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    crosapi::IdleServiceAsh::DisableForTesting();

    if (!ash::LoginState::IsInitialized()) {
      ash::LoginState::Initialize();
    }
    crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();
#endif

    // `ReadWriteCardsManagerImpl` will initialize `QuickAnswersState`
    // indirectly. `QuickAnswersState` depends on `MagicBoostState`.
    magic_boost_state_ = std::make_unique<ash::MagicBoostStateAsh>();
    manager_ = std::make_unique<ReadWriteCardsManagerImpl>();
  }

  bool IsMahiEnabled() { return GetParam(); }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    crosapi_manager_.reset();
    testing_profile_ = nullptr;
    profile_manager_.DeleteTestingProfile(chrome::kInitialProfile);
#endif
    magic_boost_state_.reset();
    manager_.reset();
    ChromeAshTestBase::TearDown();
  }

  void OnGetEditorContext(editor_menu::FetchControllersCallback callback,
                          editor_menu::EditorMode editor_mode,
                          bool editor_consent_status_settled) {
    content::ContextMenuParams params;
    params.is_editable = true;

    editor_menu::EditorContext editor_context(
        editor_mode, /*consent_status_settled=*/editor_consent_status_settled,
        {});

    manager_->OnGetEditorContext(params, std::move(callback), editor_context);
  }

  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> GetControllers(
      content::ContextMenuParams params,
      editor_menu::EditorMode editor_mode = editor_menu::EditorMode::kWrite,
      bool editor_consent_status_settled = true) {
    editor_menu::EditorContext editor_context(
        editor_mode, /*consent_status_settled=*/editor_consent_status_settled,
        {});

    return manager_->GetControllers(params, editor_context);
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

  // Providing the test crosapi manager.
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  raw_ptr<TestingProfile> testing_profile_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
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

TEST_P(ReadWriteCardsManagerImplTest, MahiNotDistillable) {
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

TEST_P(ReadWriteCardsManagerImplTest, QuickAnswersAndMahiControllersApproved) {
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

TEST_P(ReadWriteCardsManagerImplTest, QuickAnswersAndMahiControllersDeclined) {
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

TEST_P(ReadWriteCardsManagerImplTest,
       MagicBoostOptInQuickAnswerAndMahiNoSelectedText) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  magic_boost_state_->AsyncWriteConsentStatus(HMRConsentStatus::kUnset);

  content::ContextMenuParams params;

  // When Mahi is enabled and consent status is unset, the opt in card
  // controller should be fetched.
  if (IsMahiEnabled()) {
    mahi_menu_controller()->set_is_distillable_for_testing(true);

    ExpectControllersEqual(
        "Wrong quick answers/mahi controller is fetched when "
        "consent status is unset on unselected text and Mahi is enabled",
        std::vector<ReadWriteCardController*>{magic_boost_card_controller()},
        GetControllers(params));

    EXPECT_EQ(crosapi::mojom::MagicBoostController::OptInFeatures::kHmrOnly,
              magic_boost_card_controller()->GetOptInFeatures());
    EXPECT_EQ(
        crosapi::mojom::MagicBoostController::TransitionAction::kShowHmrPanel,
        magic_boost_card_controller()->transition_action_for_test());

    // When editor mode is kPromoCard, Magic Boost should opt in both Hmr and
    // Orca.
    ExpectControllersEqual(
        "",
        std::vector<ReadWriteCardController*>{magic_boost_card_controller()},
        GetControllers(params, editor_menu::EditorMode::kPromoCard,
                       /*editor_consent_status_settled=*/false));

    EXPECT_EQ(crosapi::mojom::MagicBoostController::OptInFeatures::kOrcaAndHmr,
              magic_boost_card_controller()->GetOptInFeatures());
    EXPECT_EQ(
        crosapi::mojom::MagicBoostController::TransitionAction::kShowHmrPanel,
        magic_boost_card_controller()->transition_action_for_test());
    return;
  }

  EXPECT_TRUE(GetControllers(params).empty())
      << "Wrong quick answers/mahi controller is fetched when consent status "
         "is unset on unselected text and Mahi is disabled";
}

TEST_P(ReadWriteCardsManagerImplTest,
       MagicBoostOptInQuickAnswerAndMahiSelectedText) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  content::ContextMenuParams params;
  params.selection_text = u"text";

  if (IsMahiEnabled()) {
    mahi_menu_controller()->set_is_distillable_for_testing(true);

    ExpectControllersEqual(
        "Wrong quick answers/mahi controller is fetched when "
        "consent status is unset on selected text and Mahi is enabled",
        std::vector<ReadWriteCardController*>{magic_boost_card_controller()},
        GetControllers(params));

    EXPECT_EQ(crosapi::mojom::MagicBoostController::OptInFeatures::kHmrOnly,
              magic_boost_card_controller()->GetOptInFeatures());
    EXPECT_EQ(
        crosapi::mojom::MagicBoostController::TransitionAction::kShowHmrPanel,
        magic_boost_card_controller()->transition_action_for_test());

    // When editor mode is kPromoCard, Magic Boost should opt in both Hmr and
    // Orca.
    auto controllers =
        GetControllers(params, editor_menu::EditorMode::kPromoCard,
                       /*editor_consent_status_settled=*/false);

    ExpectControllersEqual(
        "",
        std::vector<ReadWriteCardController*>{magic_boost_card_controller()},
        controllers);

    EXPECT_EQ(crosapi::mojom::MagicBoostController::OptInFeatures::kOrcaAndHmr,
              magic_boost_card_controller()->GetOptInFeatures());
    EXPECT_EQ(
        crosapi::mojom::MagicBoostController::TransitionAction::kShowHmrPanel,
        magic_boost_card_controller()->transition_action_for_test());

    return;
  }

  ExpectControllersEqual(
      "Wrong quick answers/mahi controller is fetched when consent status "
      "is unset on selected text and Mahi is disabled",
      std::vector<ReadWriteCardController*>{quick_answers_controller()},
      GetControllers(params));
}

// Tests that the appropriate controller is returned given the editor mode
// provided in each case.
TEST_P(ReadWriteCardsManagerImplTest,
       OnGetEditorContextSoftBlockedAndConsentStatusAlreadySet) {
  // If no text is selected, editor mode is kSoftBlocked and editor consent
  // status is already set, no card is shown.
  OnGetEditorContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kSoftBlocked",
          std::vector<ReadWriteCardController*>{}),
      editor_menu::EditorMode::kSoftBlocked,
      /*editor_consent_status_settled=*/true);

  if (IsMahiEnabled()) {
    EXPECT_EQ(
        crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing,
        magic_boost_card_controller()->transition_action_for_test());
  }
}

TEST_P(ReadWriteCardsManagerImplTest,
       OnGetEditorContextHardBlockedAndEditorConsentStatusUnset) {
  // If no text is selected and editor mode is kHardBlocked, no card is shown
  OnGetEditorContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kHardBlocked",
          std::vector<ReadWriteCardController*>{}),
      editor_menu::EditorMode::kHardBlocked,
      /*editor_consent_status_settled=*/false);

  if (IsMahiEnabled()) {
    EXPECT_EQ(
        crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing,
        magic_boost_card_controller()->transition_action_for_test());
  }
}

TEST_P(ReadWriteCardsManagerImplTest,
       OnGetEditorContextSoftBlockedAndEditorConsentStatusUnset) {
  OnGetEditorContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kSoftBlocked",
          IsMahiEnabled()
              ? std::vector<
                    ReadWriteCardController*>{magic_boost_card_controller()}
              : std::vector<ReadWriteCardController*>{}),
      editor_menu::EditorMode::kSoftBlocked,
      /*editor_consent_status_settled=*/false);

  if (IsMahiEnabled()) {
    EXPECT_EQ(crosapi::mojom::MagicBoostController::TransitionAction::
                  kShowEditorPanel,
              magic_boost_card_controller()->transition_action_for_test());
    EXPECT_EQ(crosapi::mojom::MagicBoostController::OptInFeatures::kOrcaAndHmr,
              magic_boost_card_controller()->GetOptInFeatures());
  }
}

TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorContextPromoCard) {
  OnGetEditorContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kPromoCard",
          IsMahiEnabled()
              ? std::vector<
                    ReadWriteCardController*>{magic_boost_card_controller()}
              : std::vector<
                    ReadWriteCardController*>{editor_menu_controller()}),
      editor_menu::EditorMode::kPromoCard,
      /*editor_consent_status_settled=*/false);

  if (IsMahiEnabled()) {
    // Should show opt-in for both Hmr and Orca.
    EXPECT_EQ(crosapi::mojom::MagicBoostController::OptInFeatures::kOrcaAndHmr,
              magic_boost_card_controller()->GetOptInFeatures());
    EXPECT_EQ(crosapi::mojom::MagicBoostController::TransitionAction::
                  kShowEditorPanel,
              magic_boost_card_controller()->transition_action_for_test());
  }
}

TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorContextWrite) {
  OnGetEditorContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kWrite",
          std::vector<ReadWriteCardController*>{editor_menu_controller()}),
      editor_menu::EditorMode::kWrite, /*editor_consent_status_settled=*/true);
}

TEST_P(ReadWriteCardsManagerImplTest, OnGetEditorContextRewrite) {
  OnGetEditorContext(
      base::BindOnce(
          &ExpectControllersEqual,
          "Wrong controller is fetched when editor mode is kRewrite",
          std::vector<ReadWriteCardController*>{editor_menu_controller()}),
      editor_menu::EditorMode::kRewrite,
      /*editor_consent_status_settled=*/true);
}

}  // namespace chromeos
