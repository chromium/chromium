// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_PROMPT_H_

#include "components/permissions/permission_prompt.h"

namespace content {
class WebContents;
}

// Factory function to create permission prompts for chrome.
std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate);

#endif  // CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_PROMPT_H_
