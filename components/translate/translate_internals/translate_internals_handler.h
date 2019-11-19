// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_HANDLER_H_
#define COMPONENTS_TRANSLATE_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/variations/service/variations_service.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}  // namespace base

namespace translate {
struct LanguageDetectionDetails;
struct TranslateErrorDetails;
struct TranslateEventDetails;
struct TranslateInitDetails;

// The handler class for chrome://translate-internals page operations.
class TranslateInternalsHandler {
 public:
  TranslateInternalsHandler();
  ~TranslateInternalsHandler();

  // Returns a dictionary of languages in |dict| where each key is a language
  // code and each value is a language name in the locale.
  static void GetLanguages(base::DictionaryValue* dict);

  virtual TranslateClient* GetTranslateClient() = 0;
  virtual variations::VariationsService* GetVariationsService() = 0;
  // Registers to handle |message| from JavaScript with |callback|.
  using MessageCallback = base::RepeatingCallback<void(const base::ListValue*)>;
  virtual void RegisterMessageCallback(const std::string& message,
                                       const MessageCallback& callback) = 0;
  // Calls a Javascript function with the given name and arguments.
  virtual void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) = 0;

 protected:
  // Subclasses should call this in order to handle messages from JavaScript.
  void RegisterMessageCallbacks();
  // Subclasses should call this when language detection details are available.
  void AddLanguageDetectionDetails(const LanguageDetectionDetails& details);

 private:
  // Callback for translate errors.
  void OnTranslateError(const TranslateErrorDetails& details);

  // Callback for translate initialisations.
  virtual void OnTranslateInit(const translate::TranslateInitDetails& details);

  // Callback for translate events.
  virtual void OnTranslateEvent(const TranslateEventDetails& details);

  // Handles the Javascript message 'removePrefItem'. This message is sent
  // when UI requests to remove an item in the preference.
  void OnRemovePrefItem(const base::ListValue* args);

  // Handles the JavaScript message 'setRecentTargetLanguage'. This message is
  // sent when the UI requests to change the 'translate_recent_target'
  // preference.
  void OnSetRecentTargetLanguage(const base::ListValue* args);

  // Handles the Javascript message 'overrideCountry'. This message is sent
  // when UI requests to override the stored country.
  void OnOverrideCountry(const base::ListValue* country);

  // Handles the Javascript message 'requestInfo'. This message is sent
  // when UI needs to show information concerned with the translation.
  // For now, this returns only prefs to Javascript.
  // |args| is not used.
  void OnRequestInfo(const base::ListValue* args);

  // Sends a message to Javascript.
  void SendMessageToJs(const std::string& message, const base::Value& value);

  // Sends the current preference to Javascript.
  void SendPrefsToJs();

  // Sends the languages currently supported by the server to JavaScript.
  void SendSupportedLanguagesToJs();

  // Sends the stored permanent country to Javascript.
  // |was_updated| tells Javascript if the country has been updated or not.
  void SendCountryToJs(bool was_updated);

  // Subscription for translate events coming from the translate language list.
  std::unique_ptr<TranslateLanguageList::EventCallbackList::Subscription>
      event_subscription_;

  // Subscription for translate errors coming from the translate manager.
  std::unique_ptr<TranslateManager::TranslateErrorCallbackList::Subscription>
      error_subscription_;

  // Subscription for translate initialization event.
  std::unique_ptr<
      translate::TranslateManager::TranslateInitCallbackList::Subscription>
      init_subscription_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInternalsHandler);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_HANDLER_H_
