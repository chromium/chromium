// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_profiles/save_address_profile_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

namespace autofill {

class SaveAddressProfileViewTest : public ChromeViewsTestBase {
 public:
  SaveAddressProfileViewTest();
  ~SaveAddressProfileViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  SaveAddressProfileView* view() { return view_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  SaveAddressProfileView* view_;
};

SaveAddressProfileViewTest::SaveAddressProfileViewTest() {
  feature_list_.InitAndEnableFeature(
      features::kAutofillAddressProfileSavePrompt);

  test_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
}

void SaveAddressProfileViewTest::CreateViewAndShow() {
  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();

  view_ = new SaveAddressProfileView(anchor_widget_->GetContentsView(),
                                     test_web_contents_.get());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(SaveAddressProfileViewTest, HasCloseButton) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowCloseButton());
}

}  // namespace autofill
