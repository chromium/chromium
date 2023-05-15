// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_ONE_ORIGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_ONE_ORIGIN_VIEW_H_

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

// Bubble that prompts the user to grant or deny a permission request from one
// origin.
//
// ----------------------------------------------
// |                                       [ X ]|
// | Prompt title                               |
// | ------------------------------------------ |
// | 1+ rows with requests                      |
// | e.g. [LocationIcon] Know your location     |
// | ------------------------------------------ |
// | Extra text                                 |
// | ------------------------------------------ |
// |                        [ Block ] [ Allow ] |
// ----------------------------------------------
class PermissionPromptBubbleOneOriginView
    : public PermissionPromptBubbleBaseView {
 public:
  PermissionPromptBubbleOneOriginView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::TimeTicks permission_requested_time,
      PermissionPromptStyle prompt_style);
  PermissionPromptBubbleOneOriginView(
      const PermissionPromptBubbleOneOriginView&) = delete;
  PermissionPromptBubbleOneOriginView& operator=(
      const PermissionPromptBubbleOneOriginView&) = delete;
  ~PermissionPromptBubbleOneOriginView() override;

 private:
  // Add a line for the |request| at |index| of the view.
  void AddRequestLine(permissions::PermissionRequest* request,
                      std::size_t index);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_ONE_ORIGIN_VIEW_H_
