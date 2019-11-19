// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/translate_internals/chrome_translate_internals_handler.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

ChromeTranslateInternalsHandler::ChromeTranslateInternalsHandler() {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                              content::NotificationService::AllSources());
}

ChromeTranslateInternalsHandler::~ChromeTranslateInternalsHandler() {}

translate::TranslateClient*
ChromeTranslateInternalsHandler::GetTranslateClient() {
  return ChromeTranslateClient::FromWebContents(web_ui()->GetWebContents());
}

variations::VariationsService*
ChromeTranslateInternalsHandler::GetVariationsService() {
  return g_browser_process->variations_service();
}

void ChromeTranslateInternalsHandler::RegisterMessageCallback(
    const std::string& message,
    const MessageCallback& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void ChromeTranslateInternalsHandler::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  web_ui()->CallJavascriptFunctionUnsafe(function_name, args);
}

void ChromeTranslateInternalsHandler::RegisterMessages() {
  RegisterMessageCallbacks();
}

void ChromeTranslateInternalsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED, type);

  content::WebContents* web_contents =
      content::Source<content::WebContents>(source).ptr();
  const translate::LanguageDetectionDetails* language_detection_details =
      content::Details<const translate::LanguageDetectionDetails>(details)
          .ptr();
  if (web_contents->GetBrowserContext()->IsOffTheRecord() ||
      !GetTranslateClient()->IsTranslatableURL(
          language_detection_details->url)) {
    return;
  }

  AddLanguageDetectionDetails(*language_detection_details);
}
