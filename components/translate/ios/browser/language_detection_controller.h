// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_member.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

class GURL;
@class JsLanguageDetectionManager;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace net {
class HttpResponseHeaders;
}

namespace web {
class NavigationContext;
}

namespace translate {

class LanguageDetectionController : public web::WebStateObserver {
 public:
  LanguageDetectionController(web::WebState* web_state,
                              JsLanguageDetectionManager* manager,
                              PrefService* prefs);
  ~LanguageDetectionController() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LanguageDetectionControllerTest, OnTextCaptured);
  FRIEND_TEST_ALL_PREFIXES(LanguageDetectionControllerTest,
                           MissingHttpContentLanguage);

  // Starts the page language detection and initiates the translation process.
  void StartLanguageDetection();

  // Handles the "languageDetection.textCaptured" javascript command.
  // |interacting| is true if the user is currently interacting with the page.
  void OnTextCaptured(const base::DictionaryValue& value,
                      const GURL& url,
                      bool user_is_interacting,
                      web::WebFrame* sender_frame);

  // Completion handler used to retrieve the text buffered by the
  // JsLanguageDetectionManager.
  void OnTextRetrieved(const std::string& http_content_language,
                       const std::string& html_lang,
                       const GURL& url,
                       const base::string16& text);

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
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  JsLanguageDetectionManager* js_manager_;
  BooleanPrefMember translate_enabled_;
  std::string content_language_header_;
  base::WeakPtrFactory<LanguageDetectionController> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(LanguageDetectionController);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
