// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_

#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/translate/core/common/translate_errors.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_user_data.h"

namespace network {
class SimpleURLLoader;
}

namespace url {
class Origin;
}

namespace web {
class WebFrame;
}

namespace translate {

// TranslateController controls the translation of the page, by injecting the
// translate scripts and monitoring the status.
class TranslateController : public web::WebStateUserData<TranslateController> {
 public:
  // Observer class to monitor the progress of the translation.
  class Observer : public base::CheckedObserver {
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

    // Called when the observed instance is being destroyed so that observers
    // can call RemoveObserver on the instance.
    virtual void TranslateControllerWasDestroyed(
        TranslateController* translate_controller) = 0;
  };

  TranslateController(const TranslateController&) = delete;
  TranslateController& operator=(const TranslateController&) = delete;

  ~TranslateController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Injects the translate script.
  void InjectTranslateScript(const std::string& translate_script);

  // Reverts the translation.
  void RevertTranslation();

  // Starts the translation. Must be called when the translation script is
  // ready.
  void StartTranslation(const std::string& source_language,
                        const std::string& target_language);

  // Called when a JavaScript command is received.
  void OnJavascriptCommandReceived(url::Origin security_origin,
                                   const base::DictValue& payload);

 private:
  TranslateController(web::WebState* web_state);
  friend class web::WebStateUserData<TranslateController>;
  friend class TranslateControllerTest;

  // Methods to handle specific JavaScript commands.
  // The command is ignored if `payload` format is unexpected.
  void OnTranslateReady(const base::DictValue& payload);
  void OnTranslateComplete(const base::DictValue& payload);
  void OnLoadJavascript(url::Origin security_origin,
                        const base::DictValue& payload);

  // The main frame of `web_state_`, if any.
  web::WebFrame* GetMainWebFrame();

  // The WebState this instance is observing.
  raw_ptr<web::WebState> web_state_;

  base::ObserverList<Observer> observers_;

  // The WebFrame ID for the frame from `web_state_` which the translate script
  // was last injected.
  std::optional<std::string> translate_script_injected_frame_id_;

  // Loader used to fetch the translate script.
  std::unique_ptr<network::SimpleURLLoader> script_loader_;

  // Called when the script is loaded.
  void OnScriptLoaded(std::string frame_id,
                      std::optional<std::string> response_body);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_CONTROLLER_H_
