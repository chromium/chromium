// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace web_modal {

class TestWebContentsModalDialogManagerDelegate
    : public WebContentsModalDialogManagerDelegate {
 public:
  TestWebContentsModalDialogManagerDelegate();

  // WebContentsModalDialogManagerDelegate overrides:
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;

  WebContentsModalDialogHost* GetWebContentsModalDialogHost() override;

  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  void set_web_contents_visible(bool visible) {
    web_contents_visible_ = visible;
  }

  void set_web_contents_modal_dialog_host(WebContentsModalDialogHost* host) {
    web_contents_modal_dialog_host_ = host;
  }

  bool web_contents_blocked() const { return web_contents_blocked_; }

 private:
  bool web_contents_visible_;
  bool web_contents_blocked_;
  WebContentsModalDialogHost* web_contents_modal_dialog_host_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsModalDialogManagerDelegate);
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
