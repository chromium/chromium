// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_field_promo_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

using ::testing::Mock;

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

// This wrapper ensures that the promo controller dies when web contents are
// destroyed. This simulates a real scenario and avoids dangling pointers.
class PromoControllerWrapper
    : public content::WebContentsUserData<PromoControllerWrapper> {
 public:
  AutofillFieldPromoControllerImpl* GetPromoController() {
    return promo_controller_.get();
  }

  void ResetPromoController() { promo_controller_.reset(); }

 private:
  friend class WebContentsUserData<PromoControllerWrapper>;

  explicit PromoControllerWrapper(content::WebContents* web_contents)
      : content::WebContentsUserData<PromoControllerWrapper>(*web_contents) {
    promo_controller_ = std::make_unique<AutofillFieldPromoControllerImpl>(
        web_contents, kAutofillStandaloneCvcSuggestionElementId);
  }
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  std::unique_ptr<AutofillFieldPromoControllerImpl> promo_controller_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PromoControllerWrapper);

class AutofillFieldPromoControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    FocusWebContentsOnMainFrame();
    ASSERT_TRUE(web_contents()->GetFocusedFrame());

    promo_view_ = std::make_unique<MockAutofillFieldPromoView>();
    PromoControllerWrapper::CreateForWebContents(web_contents());

    PromoControllerWrapper* promo_controller_wrapper =
        PromoControllerWrapper::FromWebContents(web_contents());
    promo_controller_wrapper->GetPromoController()->Show(
        gfx::RectF(0, 0, 1, 1));
    promo_controller_wrapper->GetPromoController()->SetPromoViewForTesting(
        promo_view_->GetWeakPtr());

    // The view is destroyed upon calling `Close()`, like in a real scenario.
    ON_CALL(*promo_view_, Close()).WillByDefault([this]() {
      promo_view_.reset();
    });
  }

  MockAutofillFieldPromoView* promo_view() { return promo_view_.get(); }

  AutofillFieldPromoControllerImpl* promo_controller() {
    return PromoControllerWrapper::FromWebContents(web_contents())
        ->GetPromoController();
  }

  void reset_promo_controller() {
    PromoControllerWrapper::FromWebContents(web_contents())
        ->ResetPromoController();
  }

 private:
  std::unique_ptr<MockAutofillFieldPromoView> promo_view_;
};

TEST_F(AutofillFieldPromoControllerImplTest, CloseViewOnHide) {
  EXPECT_CALL(*promo_view(), Close());
  promo_controller()->Hide();
  Mock::VerifyAndClearExpectations(promo_view());
  EXPECT_FALSE(promo_view());
}

TEST_F(AutofillFieldPromoControllerImplTest, CloseViewOnControllerDeletion) {
  EXPECT_CALL(*promo_view(), Close());
  reset_promo_controller();
  Mock::VerifyAndClearExpectations(promo_view());
  EXPECT_FALSE(promo_view());
}

// Tests that the hide helper can hide the view.
TEST_F(AutofillFieldPromoControllerImplTest, CloseViewOnWebContentsDestroyed) {
  EXPECT_CALL(*promo_view(), Close());
  DeleteContents();
  Mock::VerifyAndClearExpectations(promo_view());
  EXPECT_FALSE(promo_view());
}

}  // namespace autofill
