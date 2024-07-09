// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_

#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_observer.h"

// Bubble that prompts the user to grant or deny a permission request from from
// a pair of origins.
//
// -------------------------------------------------
// |                                          [ X ]|
// | Favicons from the two origins                 |
// | --------------------------------------------- |
// | Prompt title mentioning the requesting origin |
// | --------------------------------------------- |
// | Optional description                          |
// | Optional link                                 |
// | --------------------------------------------- |
// |                           [ Block ] [ Allow ] |
// -------------------------------------------------
class PermissionPromptBubbleTwoOriginsView
    : public PermissionPromptBubbleBaseView,
      public views::ViewObserver {
 public:
  PermissionPromptBubbleTwoOriginsView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::TimeTicks permission_requested_time,
      PermissionPromptStyle prompt_style);
  PermissionPromptBubbleTwoOriginsView(
      const PermissionPromptBubbleTwoOriginsView&) = delete;
  PermissionPromptBubbleTwoOriginsView& operator=(
      const PermissionPromptBubbleTwoOriginsView&) = delete;
  ~PermissionPromptBubbleTwoOriginsView() override;

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;

  // PermissionPromptBubbleBaseView
  void Show() override;

 private:
  void CreateFaviconRow();
  std::u16string CreateWindowTitle();

  void OnEmbeddingOriginFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& favicon_result);
  void OnRequestingOriginFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& favicon_result);

  void MaybeAddLink();

  /**
   * Returns a string for the link associated with the request in
   * |delegate|. The link is stylized via |link_range| and |link_style|.
   */
  std::optional<std::u16string> GetLink(
      gfx::Range& link_range,
      views::StyledLabel::RangeStyleInfo& link_style);

  /**
   * Returns a string for the link for a |RequestType::kStorageAccess|.
   * The link is stylized via |link_range| and |link_style|.
   */
  std::u16string GetLinkStorageAccess(
      gfx::Range& link_range,
      views::StyledLabel::RangeStyleInfo& link_style);
  void HelpCenterLinkClicked(const ui::Event& event);

  void MaybeShow();

  // ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  // The task tracker for loading favicons.
  std::unique_ptr<base::CancelableTaskTracker> favicon_tracker_;

  // Container that holds the favicon icons for the two origins.
  // Its ownership is transferred after widget creation to the views hierarchy
  // and becomes nullptr.
  std::unique_ptr<views::FlexLayoutView> favicon_container_;

  raw_ptr<views::ImageView> favicon_right_;
  raw_ptr<views::ImageView> favicon_left_;
  bool favicon_right_received_ = false;
  bool favicon_left_received_ = false;

  // Timer that waits for a short period of time before showing the prompt to
  // give the favicon service a chance to fetch the origins' favicons.
  base::OneShotTimer show_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_
