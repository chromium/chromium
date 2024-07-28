// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_tab_helper.h"

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"

ReadAnythingTabHelper::ReadAnythingTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<ReadAnythingTabHelper>(*web_contents),
      side_panel_controller_(
          std::make_unique<ReadAnythingSidePanelController>(web_contents)) {}

ReadAnythingTabHelper::~ReadAnythingTabHelper() = default;

void ReadAnythingTabHelper::CreateAndRegisterEntry() {
  side_panel_controller_->CreateAndRegisterEntry();
}

void ReadAnythingTabHelper::DeregisterEntry() {
  side_panel_controller_->DeregisterEntry();
}

void ReadAnythingTabHelper::AddPageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  side_panel_controller_->AddPageHandlerAsObserver(page_handler);
}

void ReadAnythingTabHelper::RemovePageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  side_panel_controller_->RemovePageHandlerAsObserver(page_handler);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingTabHelper);
