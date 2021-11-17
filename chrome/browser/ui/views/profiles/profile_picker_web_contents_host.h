// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WEB_CONTENTS_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WEB_CONTENTS_HOST_H_

#include "base/callback.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_delegate.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

// Class responsible for embedding a web contents in the profile picker and
// providing extra UI such as a back button.
class ProfilePickerWebContentsHost
    : public content::WebContentsDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  // Shows a screen with `url` in `contents`. If `url` is empty, it only shows
  // `contents` with its currently loaded url. If both
  // `navigation_finished_closure` and `url` is non-empty, the closure is called
  // when the navigation commits (if it never commits such as when the
  // navigation is replaced by another navigation, the closure is never called).
  virtual void ShowScreen(
      content::WebContents* contents,
      const GURL& url,
      base::OnceClosure navigation_finished_closure = base::OnceClosure()) = 0;
  // Like ShowScreen() but uses the picker WebContents.
  virtual void ShowScreenInPickerContents(
      const GURL& url,
      base::OnceClosure navigation_finished_closure = base::OnceClosure()) = 0;

  // Hides the profile picker window.
  virtual void Clear() = 0;

  // Returns whether dark colors should be used (based on native theme).
  virtual bool ShouldUseDarkColors() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WEB_CONTENTS_HOST_H_
