// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_side_panel_controller_utils.h"

CustomizeChromeTabHelper::~CustomizeChromeTabHelper() = default;

void CustomizeChromeTabHelper::CreateAndRegisterEntry() {
  DCHECK(delegate_);
  delegate_->CreateAndRegisterEntry(&WebContentsUserData::GetWebContents());
}

void CustomizeChromeTabHelper::DeregisterEntry() {
  DCHECK(delegate_);
  delegate_->DeregisterEntry(&WebContentsUserData::GetWebContents());
}

CustomizeChromeTabHelper::CustomizeChromeTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<CustomizeChromeTabHelper>(*web_contents),
      delegate_(customize_chrome::CreateDelegate()) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CustomizeChromeTabHelper);
