// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_IOS_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_IOS_TRANSLATE_DRIVER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/ios/browser/translate_controller.h"
#include "ios/web/public/web_state_observer.h"

namespace language {
class UrlLanguageHistogram;
}  // namespace language

namespace language_detection {
class LanguageDetectionModelLoaderServiceIOS;
}  // namespace language_detection

namespace web {
class WebState;
}

namespace translate {

class TranslateManager;

// Content implementation of TranslateDriver.
class IOSTranslateDriver
    : public TranslateDriver,
      public TranslateController::Observer,
      public web::WebStateObserver,
      public language::IOSLanguageDetectionTabHelper::Observer {
 public:
  IOSTranslateDriver(web::WebState* web_state,
                     language_detection::LanguageDetectionModelLoaderServiceIOS*
                         language_detection_model_service);

  IOSTranslateDriver(const IOSTranslateDriver&) = delete;
  IOSTranslateDriver& operator=(const IOSTranslateDriver&) = delete;

  ~IOSTranslateDriver() override;

  TranslateController* translate_controller() {
    return translate_controller_.get();
  }

  // Sets the translate manager and url language histogram to be used by the
  // driver and Inits the driver.
  void Initialize(language::UrlLanguageHistogram* url_language_histogram,
                  TranslateManager* translate_manager);

  void OnLanguageModelFileAvailabilityChanged(bool available);

  // web::WebStateObserver methods.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // language::IOSLanguageDetectionTabHelper::Observer.
  void OnLanguageDetermined(const LanguageDetectionDetails& details) override;
  void IOSLanguageDetectionTabHelperWasDestroyed(
      language::IOSLanguageDetectionTabHelper* tab_helper) override;

  // TranslateDriver methods.
  void OnIsPageTranslatedChanged() override;
  void OnTranslateEnabledChanged() override;
  bool IsLinkNavigation() override;
  void PrepareToTranslatePage(int page_seq_no,
                              const std::string& original_source_lang,
                              const std::string& target_lang,
                              bool triggered_from_menu) override;
  void TranslatePage(int page_seq_no,
                     const std::string& translate_script,
                     const std::string& source_lang,
                     const std::string& target_lang) override;
  void RevertTranslation(int page_seq_no) override;
  bool IsIncognito() const override;
  const std::string& GetContentsMimeType() override;
  const GURL& GetLastCommittedURL() const override;
  const GURL& GetVisibleURL() override;
  ukm::SourceId GetUkmSourceId() override;
  bool HasCurrentPage() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IOSTranslateDriverTest, TestTimeout);
  FRIEND_TEST_ALL_PREFIXES(IOSTranslateDriverTest, TestNoTimeout);

  // Called when the translation was successful.
  void TranslationDidSucceed(const std::string& source_lang,
                             const std::string& target_lang,
                             int page_seq_no,
                             const std::string& original_page_language,
                             double translation_time);

  // Returns true if the user has not navigated away and the the page is not
  // being destroyed.
  bool IsPageValid(int page_seq_no) const;

  // TranslateController::Observer methods.
  void OnTranslateScriptReady(TranslateErrors error_type,
                              double load_time,
                              double ready_time) override;
  void OnTranslateComplete(TranslateErrors error_type,
                           const std::string& source_language,
                           double translation_time) override;

  // Stops all observations.
  void StopAllObservations();

  // The translation action timed out.
  void OnTranslationTimeout(int pending_page_seq_no);

  // The WebState this instance is observing.
  raw_ptr<web::WebState> web_state_ = nullptr;

  base::WeakPtr<TranslateManager> translate_manager_;
  std::unique_ptr<TranslateController> translate_controller_;

  raw_ptr<language_detection::LanguageDetectionModelLoaderServiceIOS>
      language_detection_model_service_ = nullptr;

  // An ever-increasing sequence number of the current page, used to match up
  // translation requests with responses.
  // This matches the similar field in TranslateAgent in the renderer on other
  // platforms.
  int page_seq_no_;

  // When a translation is in progress, its page sequence number is stored in
  // |pending_page_seq_no_|.
  int pending_page_seq_no_;

  // Parameters of the current translation.
  std::string source_language_;
  std::string target_language_;

  // A timer to limit the length of translate actions.
  base::OneShotTimer timeout_timer_;

  // Web state observation.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  // LanguageDetectionTabHelper observation.
  base::ScopedObservation<language::IOSLanguageDetectionTabHelper,
                          language::IOSLanguageDetectionTabHelper::Observer>
      language_detection_observation_{this};

  base::WeakPtrFactory<IOSTranslateDriver> weak_ptr_factory_{this};
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_IOS_TRANSLATE_DRIVER_H_
