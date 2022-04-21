// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_member.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

class GURL;
class PrefService;

namespace net {
class HttpResponseHeaders;
}

namespace web {
class NavigationContext;
}

namespace translate {

class LanguageDetectionModel;

// Maximum length of the extracted text returned by |-extractTextContent|.
// Matches desktop implementation.
extern const size_t kMaxIndexChars;

class LanguageDetectionController : public web::WebStateObserver {
 public:
  LanguageDetectionController(web::WebState* web_state,
                              LanguageDetectionModel* language_detection_model,
                              PrefService* prefs);

  LanguageDetectionController(const LanguageDetectionController&) = delete;
  LanguageDetectionController& operator=(const LanguageDetectionController&) =
      delete;

  ~LanguageDetectionController() override;

 private:
  // Starts the page language detection and initiates the translation process.
  void StartLanguageDetection();

  // Handles the "languageDetection.textCaptured" javascript command.
  // |interacting| is true if the user is currently interacting with the page.
  void OnTextCaptured(const base::Value& value,
                      const GURL& url,
                      bool user_is_interacting,
                      web::WebFrame* sender_frame);

  // Completion handler used to retrieve the buffered text.
  void OnTextRetrieved(const bool has_notranslate,
                       const std::string& http_content_language,
                       const std::string& html_lang,
                       const GURL& url,
                       const base::Value* text_content);

  // Extracts "content-language" header into content_language_header_ variable.
  void ExtractContentLanguageHeader(net::HttpResponseHeaders* headers);

  // web::WebStateObserver implementation:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Subscription for JS message.
  base::CallbackListSubscription subscription_;

  LanguageDetectionModel* language_detection_model_ = nullptr;

  BooleanPrefMember translate_enabled_;
  std::string content_language_header_;
  base::WeakPtrFactory<LanguageDetectionController> weak_method_factory_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
