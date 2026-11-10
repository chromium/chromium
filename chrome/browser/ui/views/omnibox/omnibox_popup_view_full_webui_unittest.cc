// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

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
  bool is_full_webui_omnibox() const override { return true; }
};

// An OmniboxPopupHost that is not a presenter and records the calls the
// popup view makes on it.
class RecordingPopupHost : public OmniboxPopupHost {
 public:
  explicit RecordingPopupHost(std::vector<std::string>* calls)
      : calls_(calls) {}

  // OmniboxPopupHost:
  void Show() override {
    calls_->push_back("Show");
    shown_ = true;
  }
  void Hide() override {
    calls_->push_back("Hide");
    shown_ = false;
  }
  void RequestFocus() override { calls_->push_back("RequestFocus"); }
  bool IsShown() const override { return shown_; }
  OmniboxPopupWebUIBaseContent* GetWebUIContent() override { return nullptr; }
  void OnOmniboxFocused() override {}
  void SetPermissionPromptShowing(bool showing) override {}
  bool IsClosePrevented() const override { return false; }
  std::unique_ptr<OmniboxPopupDeactivationBlocker> CreateDeactivationBlocker()
      override {
    return nullptr;
  }
  bool CompletedFirstVisuallyNonEmptyPaint() override { return false; }

  void set_shown(bool shown) { shown_ = shown; }

 private:
  raw_ptr<std::vector<std::string>> calls_;
  bool shown_ = false;
};

class OmniboxPopupViewFullWebUITest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>());
  }

  void TearDown() override {
    popup_host_ = nullptr;
    popup_view_.reset();
    controller_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void CreatePopupView() {
    auto popup_host = std::make_unique<RecordingPopupHost>(&calls_);
    popup_host_ = popup_host.get();
    popup_view_ = std::make_unique<OmniboxPopupViewFullWebUI>(
        /*omnibox_view=*/nullptr, controller_.get(), /*location_bar=*/nullptr,
        presenter_delegate_, std::move(popup_host));
  }

  DummyOmniboxPopupPresenterDelegate presenter_delegate_;
  std::unique_ptr<OmniboxController> controller_;
  std::vector<std::string> calls_;
  raw_ptr<RecordingPopupHost> popup_host_ = nullptr;
  std::unique_ptr<OmniboxPopupViewFullWebUI> popup_view_;
};

TEST_F(OmniboxPopupViewFullWebUITest, OnFocusShowsThenRequestsFocus) {
  CreatePopupView();

  popup_view_->OnFocus(/*query_zps=*/false, /*select_all=*/false,
                       /*show_full_url=*/false);

  EXPECT_THAT(calls_, ElementsAre("Show", "RequestFocus"));
}

TEST_F(OmniboxPopupViewFullWebUITest, UpdatePopupAppearanceDoesNotCallHost) {
  CreatePopupView();

  popup_view_->UpdatePopupAppearance();

  EXPECT_THAT(calls_, IsEmpty());
}

TEST_F(OmniboxPopupViewFullWebUITest,
       SelectionIsPopupControlledWhileHostShown) {
  base::test::ScopedFeatureList feature_list(
      omnibox::kWebUIOmniboxPopupSelectionControl);
  CreatePopupView();
  popup_host_->set_shown(true);

  EXPECT_TRUE(popup_view_->IsSelectionPopupControlled());
}

TEST_F(OmniboxPopupViewFullWebUITest, PresenterIsNullForNonPresenterPopupHost) {
  CreatePopupView();

  EXPECT_EQ(popup_view_->presenter(), nullptr);
  EXPECT_EQ(popup_view_->popup_host(), popup_host_);
}

TEST_F(OmniboxPopupViewFullWebUITest, PresenterIsPopupHostForPresenterPopup) {
  auto popup_view = std::make_unique<OmniboxPopupViewFullWebUI>(
      /*omnibox_view=*/nullptr, controller_.get(), /*location_bar=*/nullptr,
      presenter_delegate_);

  ASSERT_NE(popup_view->presenter(), nullptr);
  EXPECT_EQ(popup_view->popup_host(), popup_view->presenter());
}

}  // namespace
