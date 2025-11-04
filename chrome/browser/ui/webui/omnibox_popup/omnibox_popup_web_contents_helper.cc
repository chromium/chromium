// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

OmniboxPopupWebContentsHelper::OmniboxPopupWebContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<OmniboxPopupWebContentsHelper>(
          *web_contents) {
  // If the WebUI Omnibox popup is hosted in a tab, find the OmniboxController
  // on the current browser window, and make it available to the OmniboxPopupUI.
  if (auto* browser = chrome::FindBrowserWithTab(web_contents)) {
    set_omnibox_controller(
        browser->window()->GetLocationBar()->GetOmniboxController());
  }
}

OmniboxPopupWebContentsHelper::~OmniboxPopupWebContentsHelper() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(OmniboxPopupWebContentsHelper);
