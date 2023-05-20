// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_

#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "components/favicon_base/favicon_types.h"

// Bubble that prompts the user to grant or deny a permission request from from
// a pair of origins.
//
// ----------------------------------------------
// |                                       [ X ]|
// | Prompt title mentioning the two origins    |
// | ------------------------------------------ |
// | Favicons from the two origins              |
// | ------------------------------------------ |
// | Extra text                                 |
// | ------------------------------------------ |
// |                        [ Block ] [ Allow ] |
// ----------------------------------------------
class PermissionPromptBubbleTwoOriginsView
    : public PermissionPromptBubbleBaseView {
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
  void AddFaviconRow();

  void OnEmbeddingOriginFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& favicon_result);
  void OnRequestingOriginFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& favicon_result);

  void MaybeShow();

  // The task tracker for loading favicons.
  std::unique_ptr<base::CancelableTaskTracker> favicon_tracker_;

  raw_ptr<views::ImageView> favicon_right_;
  raw_ptr<views::ImageView> favicon_left_;
  bool favicon_right_received_ = false;
  bool favicon_left_received_ = false;

  // Timer that waits for a short period of time before showing the prompt to
  // give the favicon service a chance to fetch the origins' favicons.
  base::OneShotTimer show_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_
