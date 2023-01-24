// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_side_panel_controller_utils.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

CustomizeChromeTabHelper::~CustomizeChromeTabHelper() = default;

void CustomizeChromeTabHelper::CreateAndRegisterEntry() {
  DCHECK(delegate_);
  delegate_->CreateAndRegisterEntry();
}

void CustomizeChromeTabHelper::DeregisterEntry() {
  DCHECK(delegate_);
  delegate_->DeregisterEntry();
}

void CustomizeChromeTabHelper::SetCustomizeChromeSidePanelVisible(
    bool visible,
    CustomizeChromeSection section) {
  DCHECK(delegate_);
  delegate_->SetCustomizeChromeSidePanelVisible(visible, section);
}

bool CustomizeChromeTabHelper::IsCustomizeChromeEntryShowing() const {
  DCHECK(delegate_);
  return delegate_->IsCustomizeChromeEntryShowing();
}

bool CustomizeChromeTabHelper::IsCustomizeChromeEntryAvailable() const {
  DCHECK(delegate_);
  return delegate_->IsCustomizeChromeEntryAvailable();
}

void CustomizeChromeTabHelper::EntryStateChanged(bool is_open) {
  entry_state_changed_callback_.Run(is_open);
}

void CustomizeChromeTabHelper::SetCallback(StateChangedCallBack callback) {
  entry_state_changed_callback_ = std::move(callback);
}

CustomizeChromeTabHelper::CustomizeChromeTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<CustomizeChromeTabHelper>(*web_contents),
      delegate_(customize_chrome::CreateDelegate(web_contents)) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CustomizeChromeTabHelper);
