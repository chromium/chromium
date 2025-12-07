// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_

#include <iterator>
#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/translate/core/common/translate_errors.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
}

namespace translate {

// TranslateController controls the translation of the page, by injecting the
// translate scripts and monitoring the status.
class TranslateController : public web::WebStateUserData<TranslateController> {
 public:
  // Observer class to monitor the progress of the translation.
  class Observer {
   public:
    // Called when the translate script is ready.
    // |error_type| Indicates error code.
    virtual void OnTranslateScriptReady(TranslateErrors error_type,
                                        double load_time,
                                        double ready_time) = 0;

    // Called when the translation is complete.
    // |error_type| Indicates error code.
    virtual void OnTranslateComplete(TranslateErrors error_type,
                                     const std::string& source_language,
                                     double translation_time) = 0;
  };

  TranslateController(const TranslateController&) = delete;
  TranslateController& operator=(const TranslateController&) = delete;

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

  // Called when a JavaScript command is received.
  void OnJavascriptCommandReceived(const base::Value::Dict& payload);

 private:
  TranslateController(web::WebState* web_state);
  friend class web::WebStateUserData<TranslateController>;

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
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnTranslateSendRequestWithValidCommand);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnTranslateSendRequestWithBadURL);
  FRIEND_TEST_ALL_PREFIXES(TranslateControllerTest,
                           OnTranslateSendRequestWithBadMethod);

  // Methods to handle specific JavaScript commands.
  // The command is ignored if `payload` format is unexpected.
  void OnTranslateReady(const base::Value::Dict& payload);
  void OnTranslateComplete(const base::Value::Dict& payload);

  // The main frame of `web_state_`, if any.
  web::WebFrame* GetMainWebFrame();

  // The WebState this instance is observing.
  raw_ptr<web::WebState> web_state_;

  raw_ptr<Observer, DanglingUntriaged> observer_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_
