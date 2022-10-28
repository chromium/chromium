// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

// Page handler for chrome://whats-new.
class WhatsNewHandler : public content::WebUIMessageHandler {
 public:
  WhatsNewHandler();
  ~WhatsNewHandler() override;
  WhatsNewHandler(const WhatsNewHandler&) = delete;
  WhatsNewHandler& operator=(const WhatsNewHandler&) = delete;

 private:
  void HandleInitialize(const base::Value::List& args);

  // Makes a request to show a HaTS survey.
  void TryShowHatsSurveyWithTimeout();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_
