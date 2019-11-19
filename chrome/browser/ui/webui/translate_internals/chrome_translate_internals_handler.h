// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_CHROME_TRANSLATE_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_CHROME_TRANSLATE_INTERNALS_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "components/translate/translate_internals/translate_internals_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for JavaScript messages for chrome://translate-internals.
class ChromeTranslateInternalsHandler
    : public translate::TranslateInternalsHandler,
      public content::WebUIMessageHandler,
      public content::NotificationObserver {
 public:
  ChromeTranslateInternalsHandler();
  ~ChromeTranslateInternalsHandler() override;

  // translate::TranslateInternalsHandler.
  translate::TranslateClient* GetTranslateClient() override;
  variations::VariationsService* GetVariationsService() override;
  void RegisterMessageCallback(const std::string& message,
                               const MessageCallback& callback) override;
  void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) override;

  // content::WebUIMessageHandler methods:
  void RegisterMessages() override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTranslateInternalsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_CHROME_TRANSLATE_INTERNALS_HANDLER_H_