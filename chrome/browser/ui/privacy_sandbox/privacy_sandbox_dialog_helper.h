// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HELPER_H_
#define CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class Profile;

// Helper class which watches |web_contents| to determine whether there is an
// appropriate opportunity to show the PrivacySandboxDialog. Consults with the
// PrivacySandboxService to determine what type of dialog, if any, to show.
// When an appropriate time is determined, calls Show() directly to the
// PrivacySandboxDialog.
class PrivacySandboxDialogHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrivacySandboxDialogHelper> {
 public:
  PrivacySandboxDialogHelper(const PrivacySandboxDialogHelper&) = delete;
  PrivacySandboxDialogHelper& operator=(const PrivacySandboxDialogHelper&) =
      delete;
  ~PrivacySandboxDialogHelper() override;

  // Returns whether |profile| needs to be shown a Privacy Sandbox dialog. If
  // this returns false, there is no need to create this helper.
  static bool ProfileRequiresDialog(Profile* profile);

 private:
  friend class content::WebContentsUserData<PrivacySandboxDialogHelper>;

  explicit PrivacySandboxDialogHelper(content::WebContents* web_contents);

  // contents::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  Profile* profile();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HELPER_H_
