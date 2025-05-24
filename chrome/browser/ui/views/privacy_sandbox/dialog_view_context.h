// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_DIALOG_VIEW_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_DIALOG_VIEW_CONTEXT_H_

#include "content/public/browser/web_contents_user_data.h"

namespace privacy_sandbox {

class BaseDialogUIDelegate;

// A context class, attached via WebContentsUserData, for WebContents
// specifically created for the privacy sandbox dialog view. It allows WebUI
// controllers to differentiate between WebContents hosted within the dialog and
// those loaded in a standard browser tab and provides access to the dialog's
// BaseDialogUIDelegate.
class DialogViewContext
    : public content::WebContentsUserData<DialogViewContext> {
 public:
  DialogViewContext(content::WebContents* contents,
                    BaseDialogUIDelegate& delegate);
  ~DialogViewContext() override;
  BaseDialogUIDelegate& GetDelegate();

 private:
  raw_ref<BaseDialogUIDelegate> delegate_;

  friend class content::WebContentsUserData<DialogViewContext>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_DIALOG_VIEW_CONTEXT_H_
