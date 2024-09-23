// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DRIVER_H_

#include <string>

#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace translate {

struct LanguageDetectionDetails;

// Interface that allows Translate core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class TranslateDriver {
 public:
  class LanguageDetectionObserver : public base::CheckedObserver {
   public:
    // Called when the observed instance is being destroyed so that observers
    // can reset their pointers to the TranslateDriver.
    virtual void OnTranslateDriverDestroyed(TranslateDriver* driver) {}

    // Called when the page language has been detected.
    virtual void OnLanguageDetermined(
        const translate::LanguageDetectionDetails& details) {}
  };

  TranslateDriver();
  TranslateDriver(TranslateDriver&&) = delete;
  TranslateDriver& operator=(TranslateDriver&&) = delete;
  TranslateDriver(const TranslateDriver&) = delete;
  TranslateDriver& operator=(const TranslateDriver&) = delete;
  virtual ~TranslateDriver();

  // Adds or removes observers.
  void AddLanguageDetectionObserver(LanguageDetectionObserver* observer);
  void RemoveLanguageDetectionObserver(LanguageDetectionObserver* observer);

  // Returns true if the current page was navigated through a link.
  virtual bool IsLinkNavigation() = 0;

  // Called when Translate is enabled or disabled.
  virtual void OnTranslateEnabledChanged() = 0;

  // Called when the page is "translated" state of the page changed.
  virtual void OnIsPageTranslatedChanged() = 0;

  // Called when a translation starts. The driver can do preparation work by
  // overriding this method.
  virtual void PrepareToTranslatePage(int page_seq_no,
                                      const std::string& original_source_lang,
                                      const std::string& target_lang,
                                      bool triggered_from_menu) {}

  // Translates the page contents from |source_lang| to |target_lang|.
  virtual void TranslatePage(int page_seq_no,
                             const std::string& translate_script,
                             const std::string& source_lang,
                             const std::string& target_lang) = 0;

  // Reverts the contents of the page to its original language.
  virtual void RevertTranslation(int page_seq_no) = 0;

  // Returns whether the user is currently operating in incognito mode.
  virtual bool IsIncognito() const = 0;

  // Returns the mime type of the current page.
  virtual const std::string& GetContentsMimeType() = 0;

  // Returns the last committed URL, or an empty GURL if there is no committed
  // URL.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns the visible URL, or an empty GURL if there is no visible URL.
  virtual const GURL& GetVisibleURL() = 0;

  // Returns the Ukm SourceId for the associated WebContents.
  virtual ukm::SourceId GetUkmSourceId() = 0;

  // Returns whether the driver has access to the current page.
  virtual bool HasCurrentPage() const = 0;

 protected:
  const base::ObserverList<LanguageDetectionObserver, true>&
  language_detection_observers() const {
    return language_detection_observers_;
  }

 private:
  base::ObserverList<LanguageDetectionObserver, true>
      language_detection_observers_;
};

}  // namespace translate

namespace base {

template <>
struct ScopedObservationTraits<
    translate::TranslateDriver,
    translate::TranslateDriver::LanguageDetectionObserver> {
  static void AddObserver(
      translate::TranslateDriver* source,
      translate::TranslateDriver::LanguageDetectionObserver* observer) {
    source->AddLanguageDetectionObserver(observer);
  }
  static void RemoveObserver(
      translate::TranslateDriver* source,
      translate::TranslateDriver::LanguageDetectionObserver* observer) {
    source->RemoveLanguageDetectionObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DRIVER_H_
