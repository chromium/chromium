// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_WEB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_WEB_CONTENTS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

class OmniboxController;

namespace content {
class WebContents;
}

// Helper class to make the OmniboxController available to the OmniboxPopupUI.
class OmniboxPopupWebContentsHelper
    : public content::WebContentsUserData<OmniboxPopupWebContentsHelper> {
 public:
  explicit OmniboxPopupWebContentsHelper(content::WebContents* web_contents);
  OmniboxPopupWebContentsHelper(const OmniboxPopupWebContentsHelper&) = delete;
  OmniboxPopupWebContentsHelper& operator=(
      const OmniboxPopupWebContentsHelper&) = delete;
  ~OmniboxPopupWebContentsHelper() override;

  void set_omnibox_controller(OmniboxController* controller) {
    omnibox_controller_ = controller;
  }
  OmniboxController* get_omnibox_controller() { return omnibox_controller_; }

 private:
  friend class content::WebContentsUserData<OmniboxPopupWebContentsHelper>;

  raw_ptr<OmniboxController> omnibox_controller_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_WEB_CONTENTS_HELPER_H_
