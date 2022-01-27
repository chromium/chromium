// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrivacySandboxDialogHandler : public content::WebUIMessageHandler {
 public:
  explicit PrivacySandboxDialogHandler(base::OnceClosure close_callback);
  ~PrivacySandboxDialogHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 protected:
  void HandleCloseDialog(const base::ListValue* args);

  base::OnceClosure close_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HANDLER_H_
