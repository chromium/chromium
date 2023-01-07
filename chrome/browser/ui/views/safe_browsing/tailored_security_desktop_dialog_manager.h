// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_MANAGER_H_

#include "base/functional/callback.h"

class Browser;

namespace safe_browsing {

// UMA histogram names for the dialogs.
const char kDisabledDialogOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopDialogDisabledOutcome";
const char kEnabledDialogOutcome[] =
    "SafeBrowsing.TailoredSecurity.ConsentedDesktopDialogEnabledOutcome";

static constexpr char kTailoredSecurityNoticeDialog[] =
    "TailoredSecurityNoticeDialog";

class TailoredSecurityDesktopDialogManager {
 public:
  TailoredSecurityDesktopDialogManager();
  ~TailoredSecurityDesktopDialogManager();

  // Creates and shows a dialog for when Tailored Security is enabled. If this
  // manager has opened any other dialogs, calling this method will close those
  // dialogs.
  void ShowEnabledDialogForBrowser(Browser* browser);

  // Creates and shows a dialog for when Tailored Security is disabled. If this
  // manager has opened any other dialogs, calling this method will close those
  // dialogs.
  void ShowDisabledDialogForBrowser(Browser* browser);

 private:
  // When the manager opens a dialog, the manager stores the callback to close
  // it here. There will only ever be one other open Tailored Security Desktop
  // dialog at any given time.
  base::OnceCallback<void()> close_dialog_callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_DESKTOP_DIALOG_MANAGER_H_
