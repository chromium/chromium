// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class TestOmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  using OmniboxPopupPresenterBase::OmniboxPopupPresenterBase;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override {
    return std::nullopt;
  }
  bool ShouldDetachWebContentsOnHide() const override { return true; }

  std::string_view GetPopupMetricPrefix() const override {
    return "TestPrefix";
  }
};

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
};

class OmniboxPopupPresenterBaseTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    presenter_ = std::make_unique<TestOmniboxPopupPresenter>(
        nullptr, dummy_delegate_, nullptr);

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    // Use TYPE_WINDOW_FRAMELESS to avoid native title bars and decorations
    // which cause platform-dependent minimum bounds constraints in unit tests.
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
  std::unique_ptr<TestOmniboxPopupPresenter> presenter_;

  raw_ptr<views::Widget> widget_ptr_ = nullptr;
};

TEST_F(OmniboxPopupPresenterBaseTest, OpenSmallThenGrowLarger) {
  widget_ptr_->SetBounds(gfx::Rect(0, 0, 300, 50));

  presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 500);

  widget_ptr_->SetBounds(gfx::Rect(0, 0, 800, 50));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 800);
}

TEST_F(OmniboxPopupPresenterBaseTest, OpenLargeThenShrink) {
  widget_ptr_->SetBounds(gfx::Rect(0, 0, 800, 50));

  presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 800);

  widget_ptr_->SetBounds(gfx::Rect(0, 0, 200, 50));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 500);
}

// Test the 4 Closure States (Allow, Deny, Out of Focus, Allow Always)
TEST_F(OmniboxPopupPresenterBaseTest, ResetsOnAllClosureStates) {
  auto test_closure = [&](const std::string& action_name) {
    presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
    EXPECT_EQ(presenter_->get_minimum_size(), gfx::Size(500, 400))
        << "Failed to open for " << action_name;

    presenter_->OnEmbeddedPermissionDialogChanged(false, gfx::Size());

    EXPECT_EQ(presenter_->get_minimum_size(), gfx::Size())
        << action_name << " failed to reset size!";
  };

  test_closure("Allow");
  test_closure("Deny/Close");
  test_closure("Out of Focus (Blur)");
  test_closure("Allow Always");
}
