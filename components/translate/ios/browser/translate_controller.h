// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_

#include <iterator>
#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/core/common/translate_errors.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "services/network/public/cpp/simple_url_loader.h"

@class JsTranslateManager;
class GURL;

namespace base {
class DictionaryValue;
}  // namespace base

namespace web {
class NavigationContext;
}  // namespace web

namespace translate {

// TranslateController controls the translation of the page, by injecting the
// translate scripts and monitoring the status.
class TranslateController : public web::WebStateObserver {
 public:
  // Observer class to monitor the progress of the translation.
  class Observer {
   public:
    // Called when the translate script is ready.
    // |error_type| Indicates error code.
    virtual void OnTranslateScriptReady(TranslateErrors::Type error_type,
                                        double load_time,
                                        double ready_time) = 0;

    // Called when the translation is complete.
    // |error_type| Indicates error code.
    virtual void OnTranslateComplete(TranslateErrors::Type error_type,
                                     const std::string& original_language,
                                     double translation_time) = 0;
  };

  TranslateController(web::WebState* web_state, JsTranslateManager* manager);
  ~TranslateController() override;

  // Sets the observer.
  void set_observer(Observer* observer) { observer_ = observer; }

  // Injects the translate script.
  void InjectTranslateScript(const std::string& translate_script);

  // Reverts the translation.
  void RevertTranslation();

  // Starts the translation. Must be called when the translation script is
  // ready.
  void StartTranslation(const std::string& source_language,
                        const std::string& target_language);

  // Changes the JsTranslateManager used by this TranslateController.
  // Only used for testing.
  void SetJsTranslateManagerForTesting(JsTranslateManager* manager);

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnJavascriptCommandReceived);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnIFrameJavascriptCommandReceived);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnTranslateScriptReadyTimeoutCalled);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnTranslateScriptReadyCalled);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest, TranslationSuccess);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest, TranslationFailure);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest, OnTranslateLoadJavascript);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest, OnTranslateSendRequest);

  // Called when a JavaScript command is received.
  bool OnJavascriptCommandReceived(const base::DictionaryValue& command,
                                   const GURL& url,
                                   bool interacting,
                                   web::WebFrame* sender_frame);
  // Methods to handle specific JavaScript commands.
  // Return false if the command is invalid.
  bool OnTranslateReady(const base::DictionaryValue& command);
  bool OnTranslateComplete(const base::DictionaryValue& command);
  bool OnTranslateLoadJavaScript(const base::DictionaryValue& command);
  bool OnTranslateSendRequest(const base::DictionaryValue& command);

  // The callback when the script is fetched or a server error occurred.
  void OnScriptFetchComplete(std::unique_ptr<std::string> response_body);
  // The callback when translate requests have completed.
  void OnRequestFetchComplete(
      std::set<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      std::string url,
      int request_id,
      std::unique_ptr<std::string> response_body);

  // web::WebStateObserver implementation:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Used to fetch translate requests. There may be multiple requests in flight.
  std::set<std::unique_ptr<network::SimpleURLLoader>> request_fetchers_;
  // Used to fetch additional scripts needed for translate.
  std::unique_ptr<network::SimpleURLLoader> script_fetcher_;

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  Observer* observer_;
  base::scoped_nsobject<JsTranslateManager> js_manager_;
  base::WeakPtrFactory<TranslateController> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateController);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_
