// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_VIEW_FACTORY_H_

#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

// Constructs the appropriate prompt bubble view for the |delegate| request.
raw_ptr<PermissionPromptBubbleBaseView> CreatePermissionPromptBubbleView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style);

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_VIEW_FACTORY_H_
