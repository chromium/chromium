// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TAB_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// An observer of WebContents that facilitates the logic for the Read Anything
// side panel. This per-tab class also owns the ReadAnythingSidePanelController.
class ReadAnythingTabHelper
    : public content::WebContentsUserData<ReadAnythingTabHelper> {
 public:
  // This class delegates the responsibility for registering and deregistering
  // the Read Anything side panel.
  class Delegate {
   public:
    virtual void CreateAndRegisterEntry() = 0;
    virtual void DeregisterEntry() = 0;
    virtual ~Delegate() = default;
  };

  ReadAnythingTabHelper(const ReadAnythingTabHelper&) = delete;
  ReadAnythingTabHelper& operator=(const ReadAnythingTabHelper&) = delete;
  ~ReadAnythingTabHelper() override;

  // Creates a WebUISidePanelView for Read Anything and registers the Read
  // Anything side panel entry.
  void CreateAndRegisterEntry();

  // Deregisters the Read Anything side panel entry.
  void DeregisterEntry();

 private:
  friend class content::WebContentsUserData<ReadAnythingTabHelper>;

  explicit ReadAnythingTabHelper(content::WebContents* web_contents);

  std::unique_ptr<Delegate> delegate_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TAB_HELPER_H_
