// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/read_anything/read_anything_tab_helper.h"

#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"

ReadAnythingTabHelper::ReadAnythingTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<ReadAnythingTabHelper>(*web_contents),
      delegate_(CreateDelegate(web_contents)) {}

ReadAnythingTabHelper::~ReadAnythingTabHelper() = default;

void ReadAnythingTabHelper::CreateAndRegisterEntry() {
  CHECK(delegate_);
  delegate_->CreateAndRegisterEntry();
}

void ReadAnythingTabHelper::DeregisterEntry() {
  CHECK(delegate_);
  delegate_->DeregisterEntry();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingTabHelper);
