// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"

#include "content/public/browser/web_contents.h"

CoreTabHelperDelegate::~CoreTabHelperDelegate() {
}

bool CoreTabHelperDelegate::CanReloadContents(
    content::WebContents* web_contents) const {
  return true;
}

bool CoreTabHelperDelegate::CanSaveContents(
    content::WebContents* web_contents) const {
  return true;
}
