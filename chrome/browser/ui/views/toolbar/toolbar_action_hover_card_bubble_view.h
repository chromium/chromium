// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_BUBBLE_VIEW_H_

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/tabs/fade_label_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/separator.h"

class ToolbarActionView;
class WebContents;

// Dialog that displays a hover card with extensions information.
class ToolbarActionHoverCardBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ToolbarActionHoverCardBubbleView);
  explicit ToolbarActionHoverCardBubbleView(ToolbarActionView* action_view);
  ToolbarActionHoverCardBubbleView(const ToolbarActionHoverCardBubbleView&) =
      delete;
  ToolbarActionHoverCardBubbleView& operator=(
      const ToolbarActionHoverCardBubbleView&) = delete;
  ~ToolbarActionHoverCardBubbleView() override;

  // Updates the hover card content for `action_controller` in `web_contents`.
  void UpdateCardContent(const ToolbarActionViewController* action_controller,
                         content::WebContents* web_contents);

  // Update the text fade to the given percent, which should be between 0 and 1.
  void SetTextFade(double percent);

  // Accessors used by tests.
  std::u16string GetTitleTextForTesting() const;
  bool IsSiteAccessSeparatorVisible() const;
  bool IsSiteAccessTitleVisible() const;
  bool IsSiteAccessDescriptionVisible() const;
  bool IsPolicySeparatorVisible() const;
  bool IsPolicyLabelVisible() const;

 private:
  friend class ToolbarActionHoverCardBubbleViewUITest;

  class FadeLabel;
  class FootnoteView;

  raw_ptr<FadeLabelView> title_label_ = nullptr;
  raw_ptr<FadeLabelView> site_access_title_label_ = nullptr;
  raw_ptr<FadeLabelView> site_access_description_label_ = nullptr;
  raw_ptr<FadeLabelView> policy_label_ = nullptr;

  raw_ptr<views::Separator> site_access_separator_;
  raw_ptr<views::Separator> policy_separator_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_BUBBLE_VIEW_H_
