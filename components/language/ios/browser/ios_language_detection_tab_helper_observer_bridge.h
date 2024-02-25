// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_IOS_BROWSER_IOS_LANGUAGE_DETECTION_TAB_HELPER_OBSERVER_BRIDGE_H_
#define COMPONENTS_LANGUAGE_IOS_BROWSER_IOS_LANGUAGE_DETECTION_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/memory/raw_ptr.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"

// Objective-C equivalent of language::IOSLanguageDetectionTabHelper::Observer.
@protocol IOSLanguageDetectionTabHelperObserving

- (void)iOSLanguageDetectionTabHelper:
            (language::IOSLanguageDetectionTabHelper*)tabHelper
                 didDetermineLanguage:
                     (const translate::LanguageDetectionDetails&)details;

@end

namespace language {

// Bridge class to observe IOSLanguageDetectionTabHelper::Observer in Obj-C.
class IOSLanguageDetectionTabHelperObserverBridge
    : IOSLanguageDetectionTabHelper::Observer {
 public:
  // |owner| will not be retained. |tab_helper| must not be null.
  IOSLanguageDetectionTabHelperObserverBridge(
      IOSLanguageDetectionTabHelper* tab_helper,
      id<IOSLanguageDetectionTabHelperObserving> owner);

  IOSLanguageDetectionTabHelperObserverBridge(
      const IOSLanguageDetectionTabHelperObserverBridge&) = delete;
  IOSLanguageDetectionTabHelperObserverBridge& operator=(
      const IOSLanguageDetectionTabHelperObserverBridge&) = delete;

  ~IOSLanguageDetectionTabHelperObserverBridge() override;

  // IOSLanguageDetectionTabHelper::Observer.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void IOSLanguageDetectionTabHelperWasDestroyed(
      IOSLanguageDetectionTabHelper* tab_helper) override;

 private:
  raw_ptr<IOSLanguageDetectionTabHelper> tab_helper_ = nullptr;
  __weak id<IOSLanguageDetectionTabHelperObserving> owner_;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_IOS_BROWSER_IOS_LANGUAGE_DETECTION_TAB_HELPER_OBSERVER_BRIDGE_H_
