// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_field_promo_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {
namespace {

using ::testing::Mock;
using ::testing::Return;

class MockAutofillFieldPromoView : public AutofillFieldPromoView {
 public:
  MOCK_METHOD(void, MakeInvisible, (), (override));

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
        web_contents, feature_engagement::kIPHAutofillAiOptInFeature,
        autofill::PopupViewViews::kAutofillStandaloneCvcSuggestionElementId);
  }
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  std::unique_ptr<AutofillFieldPromoControllerImpl> promo_controller_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillFieldPromoControllerWrapper);

class AutofillFieldPromoControllerImplTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    user_ed_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& window) {
                  return std::make_unique<MockBrowserUserEducationInterface>(
                      &window);
                }));

    BrowserWithTestWindowTest::SetUp();

    // Create the first tab so that `web_contents()` exists.
    AddTab(browser(), GURL(chrome::kChromeUINewTabURL));

    FocusMainFrameOfActiveWebContents();
    ASSERT_TRUE(web_contents()->GetFocusedFrame());

    AutofillFieldPromoControllerWrapper::CreateForWebContents(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  MockBrowserUserEducationInterface* user_education() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser()));
  }

  AutofillFieldPromoControllerImpl* autofill_field_promo_controller() {
    return AutofillFieldPromoControllerWrapper::FromWebContents(web_contents())
        ->GetAutofillFieldPromoController();
  }

  void reset_autofill_field_promo_controller() {
    AutofillFieldPromoControllerWrapper::FromWebContents(web_contents())
        ->ResetAutofillFieldPromoController();
  }

 private:
  ui::UserDataFactory::ScopedOverride user_ed_override_;
};

TEST_F(AutofillFieldPromoControllerImplTest, CloseViewOnFailingMaybeShowPromo) {
  auto promo_view = std::make_unique<MockAutofillFieldPromoView>();
  EXPECT_CALL(*user_education(), CanShowFeaturePromo)
      .WillOnce(testing::Return(user_education::FeaturePromoResult::Success()));
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo)
      .WillOnce([this, promo_view_ptr = promo_view->GetWeakPtr()](
                    user_education::FeaturePromoParams params) {
        autofill_field_promo_controller()->SetPromoViewForTesting(
            promo_view_ptr);
        std::move(params.show_promo_result_callback)
            .Run(user_education::FeaturePromoResult::kError);
      });

  EXPECT_CALL(*promo_view, MakeInvisible());

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
    EXPECT_CALL(*user_education(), CanShowFeaturePromo)
        .WillOnce(
            testing::Return(user_education::FeaturePromoResult::Success()));
    EXPECT_CALL(*user_education(), MaybeShowFeaturePromo).Times(1);
    autofill_field_promo_controller()->Show(gfx::RectF(0, 0, 1, 1));
    autofill_field_promo_controller()->SetPromoViewForTesting(
        promo_view_->GetWeakPtr());

    // There should be no more expectations set on `feature_promo_controller()`
    // after this. If you need to set further expectations, use a different test
    // fixture.
    Mock::VerifyAndClearExpectations(user_education());
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
  EXPECT_CALL(*promo_view(), MakeInvisible());
  autofill_field_promo_controller()->Hide();
}

TEST_F(AutofillFieldPromoControllerImplTestWithView,
       CloseViewOnControllerDeletion) {
  EXPECT_CALL(*promo_view(), MakeInvisible());
  reset_autofill_field_promo_controller();
}

// Tests that the hide helper can hide the view.
TEST_F(AutofillFieldPromoControllerImplTestWithView, CloseViewOnFrameDeleted) {
  EXPECT_CALL(*promo_view(), MakeInvisible());
  browser()->tab_strip_model()->CloseAllTabs();
}

}  // namespace
}  // namespace autofill
