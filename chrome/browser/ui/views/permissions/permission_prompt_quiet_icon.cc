// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_quiet_icon.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "components/permissions/permission_ui_selector.h"
#include "content/public/browser/web_contents.h"

PermissionPromptQuietIcon::PermissionPromptQuietIcon(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate) {
  content_settings::UpdateLocationBarUiForWebContents(web_contents);
}

PermissionPromptQuietIcon::~PermissionPromptQuietIcon() {
  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

permissions::PermissionPromptDisposition
PermissionPromptQuietIcon::GetPromptDisposition() const {
  return permissions::PermissionUiSelector::ShouldSuppressAnimation(
             delegate()->ReasonForUsingQuietUi())
             ? permissions::PermissionPromptDisposition::
                   LOCATION_BAR_RIGHT_STATIC_ICON
             : permissions::PermissionPromptDisposition::
                   LOCATION_BAR_RIGHT_ANIMATED_ICON;
}
