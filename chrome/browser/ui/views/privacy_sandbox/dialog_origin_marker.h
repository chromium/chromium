// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_DIALOG_ORIGIN_MARKER_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_DIALOG_ORIGIN_MARKER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace privacy_sandbox {

// A marker class attached via WebContentsUserData to WebContents specifically
// created for the Privacy Sandbox dialog view. This allows Privacy Sandbox
// WebUI controllers to differentiate between WebContents hosted within the
// dialog and those loaded in a standard browser tab.
class DialogOriginMarker
    : public content::WebContentsUserData<DialogOriginMarker> {
 public:
  ~DialogOriginMarker() override;

 private:
  explicit DialogOriginMarker(content::WebContents* contents);

  friend class content::WebContentsUserData<DialogOriginMarker>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_DIALOG_ORIGIN_MARKER_H_
