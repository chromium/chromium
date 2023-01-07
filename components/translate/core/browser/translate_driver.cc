// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_driver.h"
#include "base/observer_list.h"

namespace translate {

TranslateDriver::TranslateDriver() = default;

TranslateDriver::~TranslateDriver() {
  for (auto& observer : language_detection_observers())
    observer.OnTranslateDriverDestroyed(this);
}

void TranslateDriver::AddLanguageDetectionObserver(
    LanguageDetectionObserver* observer) {
  language_detection_observers_.AddObserver(observer);
}

void TranslateDriver::RemoveLanguageDetectionObserver(
    LanguageDetectionObserver* observer) {
  language_detection_observers_.RemoveObserver(observer);
}

}  // namespace translate
