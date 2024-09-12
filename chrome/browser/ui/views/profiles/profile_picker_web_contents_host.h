// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WEB_CONTENTS_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WEB_CONTENTS_HOST_H_

#include "base/functional/callback.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#endif

class GURL;
class ForceSigninUIError;

namespace content {
class WebContents;
class WebContentsDelegate;
}  // namespace content

namespace web_modal {
class WebContentsModalDialogHost;
}

// Type for a callback that is used to close the `ProfilePickerWebContentsHost`.
// It is the owner's responsibility to make sure that the issuing host is still
// alive and that the callback is valid, before running it.
using ClearHostClosure =
    base::StrongAlias<class ClearHostClosureTag, base::OnceClosure>;

// Class responsible for embedding a web contents in the profile picker and
// providing extra UI such as a back button.
class ProfilePickerWebContentsHost {
 public:
  // Shows a screen with `url` in `contents`. If `url` is empty, it only shows
  // `contents` with its currently loaded url. If both
  // `navigation_finished_closure` and `url` is non-empty, the closure is called
  // when the navigation commits (if it never commits such as when the
  // navigation is replaced by another navigation or if an internal page fails
  // to load, the closure is never called).
  virtual void ShowScreen(
      content::WebContents* contents,
      const GURL& url,
      base::OnceClosure navigation_finished_closure = base::OnceClosure()) = 0;
  // Like ShowScreen() but uses the picker WebContents.
  virtual void ShowScreenInPickerContents(
      const GURL& url,
      base::OnceClosure navigation_finished_closure = base::OnceClosure()) = 0;

  // Returns whether dark colors should be used (based on native theme).
  virtual bool ShouldUseDarkColors() const = 0;

  // Returns the picker WebContents.
  virtual content::WebContents* GetPickerContents() const = 0;

  virtual content::WebContentsDelegate* GetWebContentsDelegate() = 0;

  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHost() = 0;

  // Clears the current state an Shows the main screen.
  // `callback` is run when the main screen is shown.
  virtual void Reset(StepSwitchFinishedCallback callback) = 0;

  // Used as a callback of type `StepSwitchFinishedCallback`. Allows to show the
  // ForceSignin error dialog after completing a step switch.
  virtual void ShowForceSigninErrorDialog(const ForceSigninUIError& error,
                                          bool success) = 0;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Changes the visibility of the host's native toolbar, which shows a back
  // button.
  virtual void SetNativeToolbarVisible(bool visible) = 0;

  // Returns the background colors that other `content::WebContents` that are
  // rendered by this host should use to match the toolbar.
  virtual SkColor GetPreferredBackgroundColor() const = 0;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WEB_CONTENTS_HOST_H_
