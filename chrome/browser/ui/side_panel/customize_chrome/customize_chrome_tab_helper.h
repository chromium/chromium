// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_TAB_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// An observer of WebContents that facilitates the logic for the customize
// chrome side panel. This per-tab class also owns the
// CustomizeChromeSidePanelController.
class CustomizeChromeTabHelper
    : public content::WebContentsUserData<CustomizeChromeTabHelper> {
 public:
  // This class delegates the responsibility for registering and deregistering
  // the Customize Chrome side panel.
  class Delegate {
   public:
    virtual void CreateAndRegisterEntry(content::WebContents* web_contents) = 0;
    virtual void DeregisterEntry(content::WebContents* web_contents) = 0;
    virtual ~Delegate() = default;
  };

  CustomizeChromeTabHelper(const CustomizeChromeTabHelper&) = delete;
  CustomizeChromeTabHelper& operator=(const CustomizeChromeTabHelper&) = delete;
  ~CustomizeChromeTabHelper() override;

  // Creates a WebUISidePanelView for Customize Chrome and registers
  // the customize chrome side panel entry.
  void CreateAndRegisterEntry();

  // Deregisters the customize chrome side panel entry.
  void DeregisterEntry();

 private:
  friend class content::WebContentsUserData<CustomizeChromeTabHelper>;
  explicit CustomizeChromeTabHelper(content::WebContents* web_contents);

  std::unique_ptr<Delegate> delegate_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_TAB_HELPER_H_
