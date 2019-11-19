// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/ios/browser/ios_language_detection_tab_helper.h"

#include "components/language/core/browser/url_language_histogram.h"
#include "components/translate/core/common/language_detection_details.h"

namespace language {

IOSLanguageDetectionTabHelper::IOSLanguageDetectionTabHelper(
    UrlLanguageHistogram* const url_language_histogram)
    : url_language_histogram_(url_language_histogram) {}

IOSLanguageDetectionTabHelper::~IOSLanguageDetectionTabHelper() {
  for (auto& observer : observer_list_) {
    observer.IOSLanguageDetectionTabHelperWasDestroyed(this);
  }
}

// static
void IOSLanguageDetectionTabHelper::CreateForWebState(
    web::WebState* web_state,
    UrlLanguageHistogram* const url_language_histogram) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new IOSLanguageDetectionTabHelper(
                               url_language_histogram)));
  }
}

void IOSLanguageDetectionTabHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IOSLanguageDetectionTabHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IOSLanguageDetectionTabHelper::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  // Update language histogram.
  if (url_language_histogram_ && details.is_cld_reliable) {
    url_language_histogram_->OnPageVisited(details.cld_language);
  }

  for (auto& observer : observer_list_) {
    observer.OnLanguageDetermined(details);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSLanguageDetectionTabHelper)

}  // namespace language
