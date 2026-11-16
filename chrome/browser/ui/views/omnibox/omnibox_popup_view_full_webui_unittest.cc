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
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// An OmniboxPopupHost that is not a presenter and records the calls the
// popup view makes on it.
// TODO(crbug.com/585199305): Replace with a gmock mock.
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
  void ResetContentHeightForTabSwitch() override {
    calls_->push_back("ResetContentHeightForTabSwitch");
  }
  void DeactivateAndKillFocus() override {
    calls_->push_back("DeactivateAndKillFocus");
  }
  std::optional<FocusStateToRestore> TakeFocusStateToRestoreAfterModalDialog()
      override {
    return std::nullopt;
  }

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
        std::move(popup_host));
  }

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

TEST_F(OmniboxPopupViewFullWebUITest, BlurDeactivatesHostAndKillsFocus) {
  CreatePopupView();

  popup_view_->Blur();

  EXPECT_THAT(calls_, ElementsAre("DeactivateAndKillFocus"));
}

TEST_F(OmniboxPopupViewFullWebUITest, BlurOmniboxBlursInOrderAndCloses) {
  CreatePopupView();
  popup_view_->OnFocus(/*query_zps=*/false, /*select_all=*/false,
                       /*show_full_url=*/false);
  OmniboxEditModel* edit_model = controller_->edit_model();
  edit_model->SetUserText(u"");
  ASSERT_TRUE(edit_model->user_input_in_progress());
  ASSERT_TRUE(edit_model->has_focus());
  ASSERT_TRUE(popup_view_->is_focused());
  ASSERT_EQ(controller_->popup_state_manager()->popup_state(),
            OmniboxPopupState::kFull);

  bool moved_focus = false;
  OmniboxPopupViewFullWebUI::BlurOmnibox(*controller_, [&] {
    // Unedited text is reverted and the WebUI is told before the host moves
    // focus, and the edit model is blurred after.
    EXPECT_FALSE(edit_model->user_input_in_progress());
    EXPECT_FALSE(popup_view_->is_focused());
    EXPECT_TRUE(edit_model->has_focus());
    moved_focus = true;
  });

  EXPECT_TRUE(moved_focus);
  EXPECT_FALSE(edit_model->has_focus());
  EXPECT_EQ(controller_->popup_state_manager()->popup_state(),
            OmniboxPopupState::kNone);
}

TEST_F(OmniboxPopupViewFullWebUITest, BlurOmniboxKeepsPopupOpenForDraft) {
  CreatePopupView();
  popup_view_->OnFocus(/*query_zps=*/false, /*select_all=*/false,
                       /*show_full_url=*/false);
  controller_->edit_model()->SetUserText(u"draft");

  OmniboxPopupViewFullWebUI::BlurOmnibox(*controller_, [] {});

  EXPECT_FALSE(controller_->edit_model()->has_focus());
  EXPECT_EQ(controller_->popup_state_manager()->popup_state(),
            OmniboxPopupState::kFull);
}

TEST_F(OmniboxPopupViewFullWebUITest,
       BlurOmniboxWithoutPopupViewStillBlursAndCloses) {
  CreatePopupView();
  popup_view_->OnFocus(/*query_zps=*/false, /*select_all=*/false,
                       /*show_full_url=*/false);
  popup_host_ = nullptr;
  popup_view_.reset();

  OmniboxPopupViewFullWebUI::BlurOmnibox(*controller_, [] {});

  EXPECT_FALSE(controller_->edit_model()->has_focus());
  EXPECT_EQ(controller_->popup_state_manager()->popup_state(),
            OmniboxPopupState::kNone);
}

// Uses a test WebContents for `OnTabChanged()`.
class OmniboxPopupViewFullWebUITabTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>());
  }

  void TearDown() override {
    popup_view_.reset();
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<OmniboxController> controller_;
  std::vector<std::string> calls_;
  std::unique_ptr<OmniboxPopupViewFullWebUI> popup_view_;
};

// The reset hook runs only on a tab switch that shows the popup, which needs a
// connected WebUI page handler; see `OmniboxPopupPresenterBaseTest` for the
// presenter's implementation.
TEST_F(OmniboxPopupViewFullWebUITabTest,
       OnTabChangedDoesNotResetContentHeightWhenNotShowing) {
  popup_view_ = std::make_unique<OmniboxPopupViewFullWebUI>(
      /*omnibox_view=*/nullptr, controller_.get(), /*location_bar=*/nullptr,
      std::make_unique<RecordingPopupHost>(&calls_));

  popup_view_->OnTabChanged(web_contents());

  // The tab has no saved state and the WebUI isn't ready, so the popup hides.
  EXPECT_THAT(calls_, ElementsAre("Hide"));
}

}  // namespace
