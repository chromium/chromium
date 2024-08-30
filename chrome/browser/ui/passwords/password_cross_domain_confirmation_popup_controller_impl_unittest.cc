// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

namespace password_manager {
namespace {

class MockPasswordCrossDomainConfirmationPopupView
    : public PasswordCrossDomainConfirmationPopupView {
 public:
  MockPasswordCrossDomainConfirmationPopupView(
      base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
      const GURL& domain,
      const std::u16string& password_origin,
      base::OnceClosure confirmation_callback,
      base::OnceClosure cancel_callback)
      : delegate_(delegate),
        domain_(domain),
        password_origin_(password_origin),
        confirmation_callback_(std::move(confirmation_callback)),
        cancel_callback_(std::move(cancel_callback)) {}

  base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate() {
    return delegate_;
  }

  const GURL& domain() const { return domain_; }

  const std::u16string& password_origin() const { return password_origin_; }

  base::OnceClosure& confirmation_callback() { return confirmation_callback_; }

  base::OnceClosure& cancel_callback() { return cancel_callback_; }

  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, OverlapsWithPictureInPictureWindow, (), (const override));

  base::WeakPtr<MockPasswordCrossDomainConfirmationPopupView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate_;
  GURL domain_;
  std::u16string password_origin_;
  base::OnceClosure confirmation_callback_;
  base::OnceClosure cancel_callback_;

  base::WeakPtrFactory<MockPasswordCrossDomainConfirmationPopupView>
      weak_ptr_factory_{this};
};

class PasswordCrossDomainConfirmationPopupControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Required by `AutofillPopupHideHelper` for `AutofillManager`s observations
    // that indirectly rely on `ContentAutofillClient` being already created.
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents());

    // Controller requests the focused frame, this call makes sure there is one.
    FocusWebContentsOnMainFrame();

    controller_ =
        std::make_unique<PasswordCrossDomainConfirmationPopupControllerImpl>(
            web_contents());

    controller_->set_view_factory_for_testing(base::BindRepeating(
        &PasswordCrossDomainConfirmationPopupControllerImplTest::CreateView,
        base::Unretained(this)));
  }

  void TearDown() override {
    last_created_view_.reset();
    controller_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordCrossDomainConfirmationPopupControllerImpl& controller() {
    return *controller_;
  }

  MockPasswordCrossDomainConfirmationPopupView* last_created_view() {
    return last_created_view_.get();
  }

  void Show(const gfx::RectF& element_bounds = gfx::RectF(100, 100, 1000, 1000),
            base::i18n::TextDirection text_direction =
                base::i18n::TextDirection::LEFT_TO_RIGHT,
            const GURL& domain = GURL(u"google.com"),
            const std::u16string& password_origin = u"google.de",
            base::OnceClosure confirmation_callback = base::DoNothing()) {
    controller().Show(element_bounds, text_direction, domain, password_origin,
                      std::move(confirmation_callback));
  }

 private:
  base::WeakPtr<PasswordCrossDomainConfirmationPopupView> CreateView(
      base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
      const GURL& domain,
      const std::u16string& password_origin,
      base::OnceClosure confirmation_callback,
      base::OnceClosure cancel_callback) {
    last_created_view_ =
        std::make_unique<MockPasswordCrossDomainConfirmationPopupView>(
            delegate, domain, password_origin, std::move(confirmation_callback),
            std::move(cancel_callback));
    return last_created_view_->GetWeakPtr();
  }

  std::unique_ptr<PasswordCrossDomainConfirmationPopupControllerImpl>
      controller_;
  std::unique_ptr<MockPasswordCrossDomainConfirmationPopupView>
      last_created_view_;
};

}  // namespace

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       ViewIsCreatedWithShowArguments) {
  ASSERT_EQ(last_created_view(), nullptr);

  gfx::RectF element_bounds(100, 100, 1000, 1000);
  GURL domain(u"google.com");
  std::u16string password_origin(u"google.de");
  auto text_direction(base::i18n::TextDirection::LEFT_TO_RIGHT);

  Show(element_bounds, text_direction, domain, password_origin,
       base::DoNothing());

  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_EQ(last_created_view()->domain(), domain);
  EXPECT_EQ(last_created_view()->password_origin(), password_origin);
  EXPECT_EQ(controller().element_bounds(), element_bounds);
  EXPECT_EQ(controller().GetElementTextDirection(), text_direction);
}

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       PreviousViewIsHiddenOnShow) {
  Show();
  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_CALL(*last_created_view(), Hide);

  Show();
}

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       PopupIsHiddenOnConfirmation) {
  base::HistogramTester histogram_tester;
  Show();
  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_CALL(*last_created_view(), Hide);
  std::move(last_created_view()->confirmation_callback()).Run();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ManualFallback.CrossDomainPasswordFilling."
      "ConfirmationBubbleResult",
      PasswordCrossDomainConfirmationPopupControllerImpl::
          CrossDomainPasswordFillingConfirmation::kConfirmed,
      1);

  ::testing::Mock::VerifyAndClearExpectations(last_created_view());
}

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       PopupIsHiddenOnCancel) {
  base::HistogramTester histogram_tester;
  Show();
  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_CALL(*last_created_view(), Hide);
  std::move(last_created_view()->cancel_callback()).Run();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ManualFallback.CrossDomainPasswordFilling."
      "ConfirmationBubbleResult",
      PasswordCrossDomainConfirmationPopupControllerImpl::
          CrossDomainPasswordFillingConfirmation::kCanceled,
      1);

  ::testing::Mock::VerifyAndClearExpectations(last_created_view());
}

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       PopupIsHiddenOnViewDestroy) {
  base::HistogramTester histogram_tester;
  Show();
  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_CALL(*last_created_view(), Hide);

  last_created_view()->delegate()->ViewDestroyed();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ManualFallback.CrossDomainPasswordFilling."
      "ConfirmationBubbleResult",
      PasswordCrossDomainConfirmationPopupControllerImpl::
          CrossDomainPasswordFillingConfirmation::kIgnored,
      1);

  ::testing::Mock::VerifyAndClearExpectations(last_created_view());
}

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       PopupIsHiddenOnNavigation) {
  base::HistogramTester histogram_tester;
  Show();
  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_CALL(*last_created_view(), Hide);

  // This hiding is handled by `autofill::AutofillPopupHideHelper` and this test
  // basically tests integration with it.
  NavigateAndCommit(GURL("example.com"));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ManualFallback.CrossDomainPasswordFilling."
      "ConfirmationBubbleResult",
      PasswordCrossDomainConfirmationPopupControllerImpl::
          CrossDomainPasswordFillingConfirmation::kIgnored,
      1);

  ::testing::Mock::VerifyAndClearExpectations(last_created_view());
}

TEST_F(PasswordCrossDomainConfirmationPopupControllerImplTest,
       PopupIsHiddenOnUserInteraction) {
  base::HistogramTester histogram_tester;
  Show();
  ASSERT_NE(last_created_view(), nullptr);
  EXPECT_CALL(*last_created_view(), Hide);

  controller().DidGetUserInteraction(blink::SyntheticWebTouchEvent());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ManualFallback.CrossDomainPasswordFilling."
      "ConfirmationBubbleResult",
      PasswordCrossDomainConfirmationPopupControllerImpl::
          CrossDomainPasswordFillingConfirmation::kIgnored,
      1);

  ::testing::Mock::VerifyAndClearExpectations(last_created_view());
}

}  // namespace password_manager
