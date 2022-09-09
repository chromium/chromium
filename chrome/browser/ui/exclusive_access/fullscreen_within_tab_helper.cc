// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/fullscreen_within_tab_helper.h"

FullscreenWithinTabHelper::FullscreenWithinTabHelper(
    content::WebContents* contents)
    : content::WebContentsUserData<FullscreenWithinTabHelper>(*contents) {}

FullscreenWithinTabHelper::~FullscreenWithinTabHelper() {}

// static
void FullscreenWithinTabHelper::RemoveForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  web_contents->RemoveUserData(UserDataKey());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FullscreenWithinTabHelper);
