// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

class DummyOmniboxPopupPresenterDelegate
    : public OmniboxPopupPresenterDelegate {
 public:
  views::Widget* GetLocationBarWidget() override { return nullptr; }
  OmniboxPopupFileSelector* GetOmniboxPopupFileSelector() const override {
    return nullptr;
  }
  OmniboxPopupAimPresenter* GetOmniboxPopupAimPresenter() const override {
    return nullptr;
  }
  views::View* GetLocationBarFocusRestoreView() override { return nullptr; }
};

class TestOmniboxPopupFullPresenter : public OmniboxPopupFullPresenter {
 public:
  TestOmniboxPopupFullPresenter(
      OmniboxPopupPresenterDelegate& presenter_delegate,
      OmniboxController* controller)
      : OmniboxPopupFullPresenter(nullptr, presenter_delegate, controller) {}

  int content_height() const { return content_height_; }
  void set_content_height(int height) { content_height_ = height; }
};

class OmniboxPopupFullPresenterTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    auto client = std::make_unique<TestOmniboxClient>();
    controller_ = std::make_unique<OmniboxController>(std::move(client));

    presenter_ = std::make_unique<TestOmniboxPopupFullPresenter>(
        dummy_delegate_, controller_.get());

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.context = GetContext();
    auto test_widget = CreateTestWidget(std::move(params));

    widget_ptr_ = test_widget.get();
    presenter_->set_widget_for_testing(std::move(test_widget));
  }

  void TearDown() override {
    widget_ptr_ = nullptr;
    presenter_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  DummyOmniboxPopupPresenterDelegate dummy_delegate_;
  std::unique_ptr<TestOmniboxPopupFullPresenter> presenter_;
  raw_ptr<views::Widget> widget_ptr_ = nullptr;
  std::unique_ptr<OmniboxController> controller_;
};

TEST_F(OmniboxPopupFullPresenterTest, ResetsContentHeightOnHideWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kOmniboxFullWebUIHeightWorkarounds);

  presenter_->set_content_height(400);
  EXPECT_EQ(presenter_->content_height(), 400);

  presenter_->Hide();
  EXPECT_EQ(presenter_->content_height(), 1);
}

TEST_F(OmniboxPopupFullPresenterTest,
       PreservesContentHeightOnHideWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kOmniboxFullWebUIHeightWorkarounds);

  presenter_->set_content_height(400);
  EXPECT_EQ(presenter_->content_height(), 400);

  presenter_->Hide();
  EXPECT_EQ(presenter_->content_height(), 400);
}

TEST_F(OmniboxPopupFullPresenterTest,
       PreservesContentHeightOnHideWhenSizedToPreferredHeight) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{omnibox::
                                kOmniboxFullWebUISizeWebViewToPreferredHeight},
      /*disabled_features=*/{omnibox::kOmniboxFullWebUIHeightWorkarounds});

  presenter_->set_content_height(400);
  EXPECT_EQ(presenter_->content_height(), 400);

  presenter_->Hide();
  EXPECT_EQ(presenter_->content_height(), 400);
}
}  // namespace
