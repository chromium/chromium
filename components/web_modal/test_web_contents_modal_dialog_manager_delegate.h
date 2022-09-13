// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

#include "base/compiler_specific.h"

namespace web_modal {

class TestWebContentsModalDialogManagerDelegate
    : public WebContentsModalDialogManagerDelegate {
 public:
  TestWebContentsModalDialogManagerDelegate();

  TestWebContentsModalDialogManagerDelegate(
      const TestWebContentsModalDialogManagerDelegate&) = delete;
  TestWebContentsModalDialogManagerDelegate& operator=(
      const TestWebContentsModalDialogManagerDelegate&) = delete;

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
  raw_ptr<WebContentsModalDialogHost>
      web_contents_modal_dialog_host_;  // Not owned.
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
