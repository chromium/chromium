// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_WAITER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_WAITER_H_

#include <memory>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"

namespace translate {

// A helper class that allows test to block until certain translate events have
// been received from a TranslateDriver.
class TranslateWaiter : TranslateDriver::LanguageDetectionObserver,
                        ContentTranslateDriver::TranslationObserver {
 public:
  enum class WaitEvent {
    kLanguageDetermined,
    kPageTranslated,
    kIsPageTranslatedChanged
  };

  TranslateWaiter(ContentTranslateDriver* translate_driver,
                  WaitEvent wait_event);

  TranslateWaiter(const TranslateWaiter&) = delete;
  TranslateWaiter& operator=(const TranslateWaiter&) = delete;

  ~TranslateWaiter() override;

  // Blocks until an observer function matching |wait_event_| is invoked, or
  // returns immediately if one has already been observed.
  void Wait();

  // TranslateDriver::LanguageDetectionObserver:
  void OnLanguageDetermined(const LanguageDetectionDetails& details) override;

  // ContentTranslateDriver::TranslationObserver:
  void OnPageTranslated(const std::string& source_lang,
                        const std::string& translated_lang,
                        TranslateErrors error_type) override;
  void OnIsPageTranslatedChanged(content::WebContents* source) override;

 private:
  WaitEvent wait_event_;
  base::ScopedObservation<TranslateDriver,
                          TranslateDriver::LanguageDetectionObserver>
      scoped_language_detection_observation_{this};
  base::ScopedObservation<ContentTranslateDriver,
                          ContentTranslateDriver::TranslationObserver>
      scoped_translation_observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_WAITER_H_
