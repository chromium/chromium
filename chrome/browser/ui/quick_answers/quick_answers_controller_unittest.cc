// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

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

    QuickAnswersState::Get()->set_eligibility_for_testing(true);

    controller()->SetClient(std::make_unique<quick_answers::QuickAnswersClient>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        controller()->GetQuickAnswersDelegate()));
  }

  QuickAnswersControllerImpl* controller() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  // Show the quick answer or consent view (depending on the consent status).
  void ShowView(bool set_visibility = true) {
    // To show the quick answers view, its visibility must be set to 'pending'
    // first.
    if (set_visibility)
      controller()->SetPendingShowQuickAnswers();

    // Set up a companion menu before creating the QuickAnswersView.
    CreateAndShowBasicMenu();

    controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                        kDefaultTitle, {});
  }

  void ShowConsentView() {
    // We can only show the consent view if the consent has not been
    // granted, so we add a sanity check here.
    EXPECT_TRUE(QuickAnswersState::Get()->consent_status() ==
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
    QuickAnswersState::Get()->StartConsent();
    QuickAnswersState::Get()->OnConsentResult(ConsentResultType::kAllow);
  }

  void RejectConsent() {
    QuickAnswersState::Get()->StartConsent();
    QuickAnswersState::Get()->OnConsentResult(ConsentResultType::kNoThanks);
  }

  void DismissQuickAnswers() {
    controller()->DismissQuickAnswers(
        quick_answers::QuickAnswersExitPoint::kUnspecified);
  }

  QuickAnswersUiController* ui_controller() {
    return controller()->quick_answers_ui_controller();
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(QuickAnswersControllerTest, ShouldNotShowWhenFeatureNotEligible) {
  QuickAnswersState::Get()->set_eligibility_for_testing(false);
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
  EXPECT_EQ(controller()->GetVisibilityForTesting(),
            QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest,
       ShouldShowPendingQueryAfterUserAcceptsConsent) {
  ShowView();
  // Without user consent, only the user consent view should show.
  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());

  controller()->OnUserConsentResult(true);

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

  controller()->UpdateQuickAnswersAnchorBounds(BoundsWithXPosition(123));

  // We only check the 'x' position as that is guaranteed to be identical
  // between the view and the menu.
  const views::View* quick_answers_view = GetQuickAnswersView();
  EXPECT_EQ(123, quick_answers_view->GetBoundsInScreen().x());
}

TEST_F(QuickAnswersControllerTest,
       ShouldUpdateConsentViewBoundsWhenMenuBoundsChange) {
  ShowConsentView();

  controller()->UpdateQuickAnswersAnchorBounds(BoundsWithXPosition(123));

  // We only check the 'x' position as that is guaranteed to be identical
  // between the view and the menu.
  const views::View* consent_view = GetConsentView();
  EXPECT_EQ(123, consent_view->GetBoundsInScreen().x());
}
