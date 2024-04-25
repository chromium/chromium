// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_CHROME_TRANSLATE_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_CHROME_TRANSLATE_INTERNALS_HANDLER_H_

#include <string_view>

#include "base/callback_list.h"
#include "components/translate/translate_internals/translate_internals_handler.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for JavaScript messages for chrome://translate-internals.
class ChromeTranslateInternalsHandler
    : public translate::TranslateInternalsHandler,
      public content::WebUIMessageHandler {
 public:
  ChromeTranslateInternalsHandler();

  ChromeTranslateInternalsHandler(const ChromeTranslateInternalsHandler&) =
      delete;
  ChromeTranslateInternalsHandler& operator=(
      const ChromeTranslateInternalsHandler&) = delete;

  ~ChromeTranslateInternalsHandler() override;

  // translate::TranslateInternalsHandler.
  translate::TranslateClient* GetTranslateClient() override;
  variations::VariationsService* GetVariationsService() override;
  void RegisterMessageCallback(std::string_view message,
                               MessageCallback callback) override;
  void CallJavascriptFunction(std::string_view function_name,
                              base::span<const base::ValueView> args) override;

  // content::WebUIMessageHandler methods:
  void RegisterMessages() override;

  void LanguageDetected(const translate::LanguageDetectionDetails& details);

 private:
  base::CallbackListSubscription detection_subscription_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_CHROME_TRANSLATE_INTERNALS_HANDLER_H_
