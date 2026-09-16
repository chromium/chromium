// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SUSPICIOUS_SITE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SUSPICIOUS_SITE_BUBBLE_VIEW_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;

namespace gfx {
class Rect;
}  // namespace gfx

namespace tabs {
class ScopedTabModalUI;
}  // namespace tabs

namespace views {
class MdTextButton;
class StyledLabel;
}  // namespace views

// When navigating to a suspicious site flagged by Safe Browsing, this bubble
// view is displayed anchored to the location bar to warn the user
// and provide options to return to safety or mark the site as safe.
class SuspiciousSiteBubbleView : public PageInfoBubbleViewBase {
  METADATA_HEADER(SuspiciousSiteBubbleView, PageInfoBubbleViewBase)

 public:
  SuspiciousSiteBubbleView(views::BubbleAnchor anchor,
                           const gfx::Rect& anchor_rect,
                           gfx::NativeView parent_window,
                           content::WebContents* web_contents);

  SuspiciousSiteBubbleView(const SuspiciousSiteBubbleView&) = delete;
  SuspiciousSiteBubbleView& operator=(const SuspiciousSiteBubbleView&) = delete;

  ~SuspiciousSiteBubbleView() override;

  // views::BubbleDialogDelegateView:
  void OnWidgetDestroying(views::Widget* widget) override;

  views::MdTextButton* back_to_safety_button_for_testing() {
    return back_to_safety_button_;
  }
  views::MdTextButton* mark_as_safe_button_for_testing() {
    return mark_as_safe_button_;
  }
  views::StyledLabel* description_label_for_testing() {
    return description_label_;
  }

 private:
  friend class SuspiciousSiteBubbleViewTest;

  void BlockWebContents();
  void UnblockWebContents();
  BrowserWindowInterface* GetBrowser() const;

  void OnBackToSafetyClicked();
  void OnMarkAsSafeClicked();
  void OpenHelpCenter();

  raw_ptr<views::MdTextButton> back_to_safety_button_ = nullptr;
  raw_ptr<views::MdTextButton> mark_as_safe_button_ = nullptr;
  raw_ptr<views::StyledLabel> description_label_ = nullptr;

  bool is_web_contents_blocked_ = false;
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
};

// Displays the suspicious site warning bubble dialog anchored to the location
// bar.
void ShowSuspiciousSiteBubble(BrowserWindowInterface* browser,
                              content::WebContents* web_contents);
void ShowSuspiciousSiteBubble(content::WebContents* web_contents);

// Creates a suspicious site warning bubble for testing.
PageInfoBubbleViewBase* CreateSuspiciousSiteBubbleForTesting(
    gfx::NativeView parent_view,
    content::WebContents* web_contents);  // IN-TEST

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SUSPICIOUS_SITE_BUBBLE_VIEW_H_
