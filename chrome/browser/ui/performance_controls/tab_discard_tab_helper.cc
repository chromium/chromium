// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"

TabDiscardTabHelper::~TabDiscardTabHelper() = default;

TabDiscardTabHelper::TabDiscardTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<TabDiscardTabHelper>(*contents) {}

bool TabDiscardTabHelper::IsChipVisible() const {
  return was_discarded_;
}

void TabDiscardTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Pages can only be discarded while they are in the background, and we only
  // need to inform the user after they have been subsequently reloaded so it
  // is suffifient to wait for a StartNavigation event before updating this
  // variable.
  was_discarded_ = navigation_handle->ExistingDocumentWasDiscarded();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabDiscardTabHelper);
