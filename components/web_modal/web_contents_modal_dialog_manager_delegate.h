// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "components/web_modal/web_modal_export.h"

namespace content {
class WebContents;
}

namespace web_modal {

class WebContentsModalDialogHost;

class WEB_MODAL_EXPORT WebContentsModalDialogManagerDelegate {
 public:
  // Changes the blocked state of |web_contents|. WebContentses are considered
  // blocked while displaying a web contents modal dialog. During that time
  // renderer host will ignore any UI interaction within WebContents outside of
  // the currently displaying dialog.
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked);

  // Returns the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs within the browser window.
  virtual WebContentsModalDialogHost* GetWebContentsModalDialogHost();

  // Returns whether the WebContents is currently visible or not.
  virtual bool IsWebContentsVisible(content::WebContents* web_contents);

 protected:
  virtual ~WebContentsModalDialogManagerDelegate();
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
