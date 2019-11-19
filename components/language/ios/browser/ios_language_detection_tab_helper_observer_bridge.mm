// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace language {

IOSLanguageDetectionTabHelperObserverBridge::
    IOSLanguageDetectionTabHelperObserverBridge(
        IOSLanguageDetectionTabHelper* tab_helper,
        id<IOSLanguageDetectionTabHelperObserving> owner)
    : tab_helper_(tab_helper), owner_(owner) {
  DCHECK(tab_helper_);
  tab_helper_->AddObserver(this);
}

IOSLanguageDetectionTabHelperObserverBridge::
    ~IOSLanguageDetectionTabHelperObserverBridge() {
  if (tab_helper_) {
    tab_helper_->RemoveObserver(this);
  }
}

void IOSLanguageDetectionTabHelperObserverBridge::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  [owner_ iOSLanguageDetectionTabHelper:tab_helper_
                   didDetermineLanguage:details];
}

void IOSLanguageDetectionTabHelperObserverBridge::
    IOSLanguageDetectionTabHelperWasDestroyed(
        IOSLanguageDetectionTabHelper* tab_helper) {
  DCHECK_EQ(tab_helper_, tab_helper);
  tab_helper_->RemoveObserver(this);
  tab_helper_ = nullptr;
}

}  // namespace language
