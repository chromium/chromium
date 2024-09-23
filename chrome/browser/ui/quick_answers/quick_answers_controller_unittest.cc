// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/quick_answers/test/mock_quick_answers_client.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/magic_boost/test/fake_magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/test/button_test_api.h"

namespace {

constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));
constexpr char kDefaultTitle[] = "default_title";

gfx::Rect BoundsWithXPosition(int x) {
  constexpr int kAnyValue = 100;
  return gfx::Rect(x, /*y=*/kAnyValue, /*width=*/kAnyValue,
                   /*height=*/kAnyValue);
}

}  // namespace

class QuickAnswersControllerTest : public ChromeQuickAnswersTestBase {
 protected:
  QuickAnswersControllerTest() = default;
  QuickAnswersControllerTest(const QuickAnswersControllerTest&) = delete;
  QuickAnswersControllerTest& operator=(const QuickAnswersControllerTest&) =
      delete;
  ~QuickAnswersControllerTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    QuickAnswersState::Get()->SetEligibilityForTesting(true);

    controller()->SetClient(
        std::make_unique<quick_answers::MockQuickAnswersClient>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            controller()->GetQuickAnswersDelegate()));
    mock_quick_answers_client_ =
        static_cast<quick_answers::MockQuickAnswersClient*>(
            controller()->GetClient());
  }

  void TearDown() override {
    // `MockQuickAnswersClient` is owned by the controller. Reset the pointer to
    // avoid a dangling pointer.
    mock_quick_answers_client_ = nullptr;

    ChromeQuickAnswersTestBase::TearDown();
  }

  QuickAnswersControllerImpl* controller() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  // Show the quick answer or consent view (depending on the consent status).
  void ShowView(bool set_visibility = true) {
    // To show the quick answers view, its visibility must be set to 'pending'
    // first.
    if (set_visibility) {
      controller()->OnContextMenuShown(GetProfile());
    }

    // Set up a companion menu before creating the QuickAnswersView.
    CreateAndShowBasicMenu();

    controller()->OnTextAvailable(kDefaultAnchorBoundsInScreen, kDefaultTitle,
                                  /*surrounding_text=*/"");
  }

  void ShowConsentView() {
    // We can only show the consent view if the consent has not been
    // granted, so we add a sanity check here.
    EXPECT_EQ(QuickAnswersState::GetConsentStatus(),
              quick_answers::prefs::ConsentStatus::kUnknown)
        << "Can not show consent view as the user consent has already "
           "been given.";
    ShowView();
  }

  const views::View* GetQuickAnswersView() const {
    return static_cast<QuickAnswersControllerImpl*>(
               QuickAnswersController::Get())
        ->quick_answers_ui_controller()
        ->quick_answers_view();
  }

  const views::View* GetConsentView() const {
    return static_cast<QuickAnswersControllerImpl*>(
               QuickAnswersController::Get())
        ->quick_answers_ui_controller()
        ->user_consent_view();
  }

  void AcceptConsent() {
    QuickAnswersState::Get()->AsyncSetConsentStatus(
        quick_answers::prefs::ConsentStatus::kAccepted);
  }

  void RejectConsent() {
    QuickAnswersState::Get()->AsyncSetConsentStatus(
        quick_answers::prefs::ConsentStatus::kRejected);
  }

  void DismissQuickAnswers() {
    controller()->DismissQuickAnswers(
        quick_answers::QuickAnswersExitPoint::kUnspecified);
  }

  QuickAnswersUiController* ui_controller() {
    return controller()->quick_answers_ui_controller();
  }

 protected:
  raw_ptr<quick_answers::MockQuickAnswersClient> mock_quick_answers_client_ =
      nullptr;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(QuickAnswersControllerTest, ShouldNotShowWhenFeatureNotEligible) {
  QuickAnswersState::Get()->SetEligibilityForTesting(false);
  ShowView();

  // The feature is not eligible, nothing should be shown.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest, ShouldNotShowWithoutSetPending) {
  ShowView(/*set_visibility=*/false);

  // The visibility has not been set to pending, nothing should be shown.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());
  EXPECT_EQ(controller()->GetQuickAnswersVisibility(),
            QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest,
       ShouldShowPendingQueryAfterUserAcceptsConsent) {
  ShowView();
  // Without user consent, only the user consent view should show.
  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());

  // Click on the "Allow" button.
  views::test::ButtonTestApi(
      ui_controller()->user_consent_view()->allow_button_for_test())
      .NotifyClick(ui::test::TestEvent());

  // With user consent granted, the consent view should dismiss and the cached
  // quick answer query should show.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest, ShouldDismissIfUserRejectConsent) {
  ShowView();
  // Without user consent, only the user consent view should show.
  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());

  controller()->OnUserConsentResult(false);

  // With user consent rejected, the views should dismiss.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest, UserConsentAlreadyAccepted) {
  AcceptConsent();
  ShowView();

  // With user consent already accepted, only the quick answers view should
  // show.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest, UserConsentAlreadyRejected) {
  RejectConsent();
  ShowView();

  // With user consent already rejected, nothing should show.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest, DismissUserConsentView) {
  ShowConsentView();
  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());

  DismissQuickAnswers();

  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
}

TEST_F(QuickAnswersControllerTest, NoUserConsentView) {
  // Note that `kMahi` is associated with the Magic Boost feature.
  // `chromeos::features::IsMagicBoostEnabled()` is only accessible from Ash
  // build. This test code is currently only included by Ash build.
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {chromeos::features::kMahi, chromeos::features::kFeatureManagementMahi},
      {});

  chromeos::test::FakeMagicBoostState fake_magic_boost_state;
  fake_magic_boost_state.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kUnset);

  ASSERT_EQ(QuickAnswersState::FeatureType::kHmr,
            QuickAnswersState::GetFeatureType());
  ASSERT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            QuickAnswersState::GetConsentStatusAs(
                QuickAnswersState::FeatureType::kQuickAnswers));
  ASSERT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            QuickAnswersState::GetConsentStatusAs(
                QuickAnswersState::FeatureType::kHmr));

  ShowConsentView();

  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView())
      << "No consent UI should be shown for kHmr as it should be handled by "
         "MagicBoost";
}

TEST_F(QuickAnswersControllerTest, DismissQuickAnswersView) {
  AcceptConsent();
  ShowView();
  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());

  DismissQuickAnswers();
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest,
       ShouldUpdateQuickAnswersViewBoundsWhenMenuBoundsChange) {
  AcceptConsent();
  ShowView();

  controller()->read_write_cards_ui_controller().SetContextMenuBounds(
      BoundsWithXPosition(123));

  // We only check the 'x' position as that is guaranteed to be identical
  // between the view and the menu.
  const views::View* quick_answers_view = GetQuickAnswersView();
  EXPECT_EQ(123, quick_answers_view->GetBoundsInScreen().x());
}

TEST_F(QuickAnswersControllerTest,
       ShouldUpdateConsentViewBoundsWhenMenuBoundsChange) {
  ShowConsentView();

  controller()->read_write_cards_ui_controller().SetContextMenuBounds(
      BoundsWithXPosition(123));

  // We only check the 'x' position as that is guaranteed to be identical
  // between the view and the menu.
  const views::View* consent_view = GetConsentView();
  EXPECT_EQ(123, consent_view->GetBoundsInScreen().x());
}

TEST_F(QuickAnswersControllerTest, ShouldNotCrashWhenContextMenuCloses) {
  ShowConsentView();

  auto* active_menu_controller = views::MenuController::GetActiveInstance();
  // Ensure that the context menu currently exists and has a non-null owner.
  ASSERT_TRUE(active_menu_controller != nullptr);
  ASSERT_TRUE(active_menu_controller->owner() != nullptr);

  // Simulate closing the context menu.
  ChromeQuickAnswersTestBase::ResetMenuParent();

  // Simulate returning a quick answers request after the context menu closed.
  // This should *not* result in a crash.
  std::unique_ptr<quick_answers::QuickAnswersRequest> processed_request =
      std::make_unique<quick_answers::QuickAnswersRequest>();
  processed_request->selected_text = "unfathomable";
  quick_answers::PreprocessedOutput expected_processed_output;
  expected_processed_output.intent_info.intent_text = "unfathomable";
  expected_processed_output.query = "Define unfathomable";
  expected_processed_output.intent_info.intent_type =
      quick_answers::IntentType::kDictionary;
  processed_request->preprocessed_output = expected_processed_output;
  controller()->OnRequestPreprocessFinished(*processed_request);

  // Confirm that the quick answers views are not showing.
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersControllerTest, NullptrResultReceived) {
  AcceptConsent();
  ShowView();

  controller()->OnQuickAnswerReceived(nullptr);

  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
  EXPECT_EQ(kDefaultTitle, base::UTF16ToUTF8(ui_controller()
                                                 ->quick_answers_view()
                                                 ->GetResultViewForTesting()
                                                 ->GetFirstLineText()));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_NO_RESULT_V2),
            ui_controller()
                ->quick_answers_view()
                ->GetResultViewForTesting()
                ->GetSecondLineText())
      << "Expect that no result UI is shown";
}

TEST_F(QuickAnswersControllerTest, PartialNullptrResultReceived) {
  AcceptConsent();
  ShowView();

  std::unique_ptr<quick_answers::QuickAnswersSession> quick_answers_session =
      std::make_unique<quick_answers::QuickAnswersSession>();
  ASSERT_FALSE(quick_answers_session->structured_result)
      << "Test the case structured_result is nullptr";
  controller()->OnQuickAnswerReceived(std::move(quick_answers_session));

  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
  EXPECT_EQ(kDefaultTitle, base::UTF16ToUTF8(ui_controller()
                                                 ->quick_answers_view()
                                                 ->GetResultViewForTesting()
                                                 ->GetFirstLineText()));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_NO_RESULT_V2),
            ui_controller()
                ->quick_answers_view()
                ->GetResultViewForTesting()
                ->GetSecondLineText())
      << "Expect that no result UI is shown";
}

TEST_F(QuickAnswersControllerTest, IntentTypeConversion) {
  QuickAnswersState::Get()->set_use_text_annotator_for_testing();

  ON_CALL(*mock_quick_answers_client_, SendRequestForPreprocessing)
      .WillByDefault(
          [this](
              const quick_answers::QuickAnswersRequest& quick_answers_request) {
            quick_answers::QuickAnswersRequest processed_request =
                quick_answers_request;
            processed_request.preprocessed_output.query =
                "Define " + quick_answers_request.selected_text;
            processed_request.preprocessed_output.intent_info.intent_type =
                quick_answers::IntentType::kDictionary;
            controller()->OnRequestPreprocessFinished(processed_request);
          });

  AcceptConsent();
  ShowView();

  EXPECT_EQ(ui_controller()->quick_answers_view()->GetIntent(),
            quick_answers::Intent::kDefinition)
      << "Quick Answers view's intent should be set to kDefinition because of "
         "the intent type from pre-process result";
}

// This is testing the case text annotator is not used, i.e., no intent is set
// from pre-process. On prod, text annotator is always used. This is the case
// for Linux-ChromeOS.
TEST_F(QuickAnswersControllerTest, IntentTypeUnknown) {
  AcceptConsent();
  ShowView();

  EXPECT_EQ(ui_controller()->quick_answers_view()->GetIntent(), std::nullopt)
      << "Intent is expected to be set std::nullopt, i.e. kUnknown";
}
