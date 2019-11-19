// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANUGUAGE_IOS_BROWSER_IOS_LANGUAGE_DETECTION_TAB_HELPER_H_
#define COMPONENTS_LANUGUAGE_IOS_BROWSER_IOS_LANGUAGE_DETECTION_TAB_HELPER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/web/public/web_state_user_data.h"

namespace translate {
struct LanguageDetectionDetails;
}  // namespace translate

namespace language {

class UrlLanguageHistogram;

// Dispatches language detection messages to language and translate components.
class IOSLanguageDetectionTabHelper
    : public web::WebStateUserData<IOSLanguageDetectionTabHelper> {
 public:
  class Observer {
   public:
    // Called when language detection details become available.
    virtual void OnLanguageDetermined(
        const translate::LanguageDetectionDetails& details) = 0;
    // Called when the observed instance is being destroyed so that observers
    // can call RemoveObserver on the instance.
    virtual void IOSLanguageDetectionTabHelperWasDestroyed(
        IOSLanguageDetectionTabHelper* tab_helper) = 0;

    virtual ~Observer() {}
  };

  ~IOSLanguageDetectionTabHelper() override;

  // Adds or Removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Attach a new helper to the given WebState. We cannot use the implementation
  // from WebStateUserData as we are injecting the histogram and translate
  // callback differently on iOS and iOS WebView.
  static void CreateForWebState(web::WebState* web_state,
                                UrlLanguageHistogram* url_language_histogram);

  // Called on page language detection.
  void OnLanguageDetermined(const translate::LanguageDetectionDetails& details);

 private:
  base::ObserverList<Observer, true>::Unchecked observer_list_;

  IOSLanguageDetectionTabHelper(
      UrlLanguageHistogram* const url_language_histogram);
  friend class web::WebStateUserData<IOSLanguageDetectionTabHelper>;

  UrlLanguageHistogram* const url_language_histogram_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(IOSLanguageDetectionTabHelper);
};

}  // namespace language

#endif  // COMPONENTS_LANUGUAGE_IOS_BROWSER_IOS_LANGUAGE_DETECTION_TAB_HELPER_H_
