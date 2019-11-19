// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_CLIENT_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"

class GURL;
class PrefService;

namespace infobars {
class InfoBar;
}  // namespace infobars

namespace translate {

class TranslateAcceptLanguages;
class TranslateDriver;
class TranslateInfoBarDelegate;

// A client interface that needs to be supplied to TranslateManager by the
// embedder.
//
// Each client instance is associated with a given context within which a
// TranslateManager is used (e.g. a single tab).
class TranslateClient {
 public:
  virtual ~TranslateClient() {}

  // Gets the TranslateDriver associated with the client.
  virtual TranslateDriver* GetTranslateDriver() = 0;

  // Returns the associated PrefService.
  virtual PrefService* GetPrefs() = 0;

  // Returns the associated TranslatePrefs.
  virtual std::unique_ptr<TranslatePrefs> GetTranslatePrefs() = 0;

  // Returns the associated TranslateAcceptLanguages.
  virtual TranslateAcceptLanguages* GetTranslateAcceptLanguages() = 0;

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Returns a translate infobar that owns |delegate|.
  virtual std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<TranslateInfoBarDelegate> delegate) const = 0;

  // Returns the resource ID of the icon to be shown for the Translate infobars.
  virtual int GetInfobarIconID() const = 0;
#endif

  // Called when the embedder should present UI to the user corresponding to the
  // user's current |step|. Returns false if the method decided not to show the
  // UI (e.g. because of TranslateRanker overrides, or same-language
  // navigation).
  virtual bool ShowTranslateUI(translate::TranslateStep step,
                               const std::string& source_language,
                               const std::string& target_language,
                               TranslateErrors::Type error_type,
                               bool triggered_from_menu) = 0;

  // Returns true if the URL can be translated.
  virtual bool IsTranslatableURL(const GURL& url) = 0;

  // Presents |report_url|, a URL containing information relating to reporting
  // a language detection error, to the user to allow them to report language
  // detection errors as desired.
  virtual void ShowReportLanguageDetectionErrorUI(const GURL& report_url) = 0;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_CLIENT_H_
