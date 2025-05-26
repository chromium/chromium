// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Property;
using ::ui::ImageModel;

class CookieControlsContentViewBrowserTest : public InProcessBrowserTest {
 public:
  CookieControlsContentViewBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Initializing view_ here, and not in constructor(), as
    // CookieControlsContentView needs the browser's UI environment.
    view_ = std::make_unique<CookieControlsContentView>();
  }

  void TearDownOnMainThread() override {
    // Resetting view_ here, not in destructor, as CookieControlsContentView
    // destruction needs live services (e.g., accessibility) to prevent
    // crashes during teardown.
    view_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  views::View* GetFeedbackButton() {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        CookieControlsContentView::kFeedbackButton,
        views::ElementTrackerViews::GetContextForView(
            view_->feedback_section_));
  }

  views::ToggleButton* GetToggleButton() { return view_->toggle_button_; }
  CookieControlsContentView* GetContentView() { return view_.get(); }

  std::unique_ptr<CookieControlsContentView> view_;
};

namespace {

IN_PROC_BROWSER_TEST_F(CookieControlsContentViewBrowserTest, FeedbackSection) {
  EXPECT_THAT(
      GetFeedbackButton()->GetViewAccessibility().GetCachedName(),
      Eq(base::JoinString(
          {l10n_util::GetStringUTF16(
               IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_BUTTON_TITLE),
           l10n_util::GetStringUTF16(
               IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_BUTTON_DESCRIPTION)},
          u"\n")));
}

IN_PROC_BROWSER_TEST_F(CookieControlsContentViewBrowserTest,
                       ToggleButton_Initial) {
  EXPECT_THAT(GetToggleButton()->GetViewAccessibility().GetCachedName(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL)));
}

IN_PROC_BROWSER_TEST_F(CookieControlsContentViewBrowserTest,
                       ToggleButton_UpdatedSites) {
  const std::u16string label = u"17 sites allowed";
  GetContentView()->SetCookiesLabel(label);
  std::u16string expected = base::JoinString(
      {l10n_util::GetStringUTF16(
           IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL),
       label},
      u"\n");
  // TODO: convert to AllOf(HasSubstr(), HasSubStr()) when gtest supports
  // u16string.
  EXPECT_THAT(GetToggleButton()->GetViewAccessibility().GetCachedName(),
              Eq(expected));
}

}  // namespace
