// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

class IntroHandler : public content::WebUIMessageHandler {
 public:
  explicit IntroHandler(base::RepeatingCallback<void(bool sign_in)> callback);

  IntroHandler(const IntroHandler&) = delete;
  IntroHandler& operator=(const IntroHandler&) = delete;

  ~IntroHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleContinueWithAccount(const base::Value::List& args);
  void HandleContinueWithoutAccount(const base::Value::List& args);

  const base::RepeatingCallback<void(bool sign_in)> callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_
