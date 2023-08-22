// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "content/public/browser/web_contents.h"

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate) {}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() = default;
