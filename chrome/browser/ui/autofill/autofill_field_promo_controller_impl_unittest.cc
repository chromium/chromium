// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_field_promo_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {
namespace {

using ::testing::Mock;
using ::testing::Return;
using user_education::test::MockFeaturePromoController;

class MockAutofillFieldPromoView : public AutofillFieldPromoView {
 public:
  MOCK_METHOD(void, Close, (), (override));

  bool OverlapsWithPictureInPictureWindow() const override { return false; }

  base::WeakPtr<AutofillFieldPromoView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<AutofillFieldPromoView> weak_ptr_factory_{this};
};

// This wrapper ensures that the autofill field promo controller dies when web
// contents are destroyed. This simulates a real scenario and avoids dangling
// pointers.
class AutofillFieldPromoControllerWrapper
    : public content::WebContentsUserData<AutofillFieldPromoControllerWrapper> {
 public:
  AutofillFieldPromoControllerImpl* GetAutofillFieldPromoController() {
    return promo_controller_.get();
  }

  void ResetAutofillFieldPromoController() { promo_controller_.reset(); }

 private:
  friend class WebContentsUserData<AutofillFieldPromoControllerWrapper>;

  explicit AutofillFieldPromoControllerWrapper(
      content::WebContents* web_contents)
      : content::WebContentsUserData<AutofillFieldPromoControllerWrapper>(
            *web_contents) {
    promo_controller_ = std::make_unique<AutofillFieldPromoControllerImpl>(
        web_contents, feature_engagement::kIPHAutofillManualFallbackFeature,
        kAutofillStandaloneCvcSuggestionElementId);
  }
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  std::unique_ptr<AutofillFieldPromoControllerImpl> promo_controller_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillFieldPromoControllerWrapper);

class AutofillFieldPromoControllerImplTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Create the first tab so that `web_contents()` exists.
    AddTab(browser(), GURL(chrome::kChromeUINewTabURL));

    FocusMainFrameOfActiveWebContents();
    ASSERT_TRUE(web_contents()->GetFocusedFrame());

    static_cast<TestBrowserWindow*>(window())->SetFeaturePromoController(
        std::make_unique<MockFeaturePromoController>());

    AutofillFieldPromoControllerWrapper::CreateForWebContents(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  MockFeaturePromoController* feature_promo_controller() {
    return static_cast<MockFeaturePromoController*>(
        static_cast<TestBrowserWindow*>(window())
            ->GetFeaturePromoControllerForTesting());
  }

  AutofillFieldPromoControllerImpl* autofill_field_promo_controller() {
    return AutofillFieldPromoControllerWrapper::FromWebContents(web_contents())
        ->GetAutofillFieldPromoController();
  }

  void reset_autofill_field_promo_controller() {
    AutofillFieldPromoControllerWrapper::FromWebContents(web_contents())
        ->ResetAutofillFieldPromoController();
  }
};

TEST_F(AutofillFieldPromoControllerImplTest, CloseViewOnFailingMaybeShowPromo) {
  auto promo_view = std::make_unique<MockAutofillFieldPromoView>();
  EXPECT_CALL(*feature_promo_controller(), MaybeShowPromo)
      .WillOnce([this, promo_view_ptr = promo_view->GetWeakPtr()](
                    user_education::FeaturePromoParams params) {
        autofill_field_promo_controller()->SetPromoViewForTesting(
            promo_view_ptr);
        std::move(params.show_promo_result_callback)
            .Run(user_education::FeaturePromoResult::kError);
      });

  EXPECT_CALL(*promo_view, Close());

  autofill_field_promo_controller()->Show(gfx::RectF(0, 0, 1, 1));
  Mock::VerifyAndClearExpectations(promo_view.get());
}

class AutofillFieldPromoControllerImplTestWithView
    : public AutofillFieldPromoControllerImplTest {
 public:
  void SetUp() override {
    AutofillFieldPromoControllerImplTest::SetUp();

    promo_view_ = std::make_unique<MockAutofillFieldPromoView>();

    // Makes sure the promo is not hidden immediately after being shown.
    // This also makes sure that `AutofillFieldPromoControllerImpl::Show()`
    // reaches `MaybeShowFeaturePromo()` and, therefore, doesn't return early.
    EXPECT_CALL(*feature_promo_controller(), MaybeShowPromo).Times(1);
    autofill_field_promo_controller()->Show(gfx::RectF(0, 0, 1, 1));
    autofill_field_promo_controller()->SetPromoViewForTesting(
        promo_view_->GetWeakPtr());

    // There should be no more expectations set on `feature_promo_controller()`
    // after this. If you need to set further expectations, use a different test
    // fixture.
    Mock::VerifyAndClearExpectations(feature_promo_controller());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(promo_view());
    AutofillFieldPromoControllerImplTest::TearDown();
  }

  MockAutofillFieldPromoView* promo_view() { return promo_view_.get(); }

 private:
  std::unique_ptr<MockAutofillFieldPromoView> promo_view_;
};

TEST_F(AutofillFieldPromoControllerImplTestWithView, CloseViewOnHide) {
  EXPECT_CALL(*promo_view(), Close());
  autofill_field_promo_controller()->Hide();
}

TEST_F(AutofillFieldPromoControllerImplTestWithView,
       CloseViewOnControllerDeletion) {
  EXPECT_CALL(*promo_view(), Close());
  reset_autofill_field_promo_controller();
}

// Tests that the hide helper can hide the view.
TEST_F(AutofillFieldPromoControllerImplTestWithView, CloseViewOnFrameDeleted) {
  EXPECT_CALL(*promo_view(), Close());
  browser()->tab_strip_model()->CloseAllTabs();
}

}  // namespace
}  // namespace autofill
