// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_CLIENT_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/translate_event.pb.h"

namespace translate {

namespace testing {

extern const char* preferred_languages_prefs;
extern const char* accept_languages_prefs;

class MockTranslateClient : public TranslateClient {
 public:
  MockTranslateClient(TranslateDriver* driver, PrefService* prefs);

  ~MockTranslateClient() override;

  TranslateDriver* GetTranslateDriver() override;

  PrefService* GetPrefs() override;

  std::unique_ptr<TranslatePrefs> GetTranslatePrefs() override;

  MOCK_METHOD0(GetTranslateAcceptLanguages, TranslateAcceptLanguages*());
  MOCK_CONST_METHOD0(GetInfobarIconID, int());

#if !defined(USE_AURA)
  MOCK_CONST_METHOD1(CreateInfoBarMock,
                     infobars::InfoBar*(TranslateInfoBarDelegate*));
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<TranslateInfoBarDelegate> delegate) const {
    return base::WrapUnique(CreateInfoBarMock(delegate.get()));
  }
#endif

  MOCK_METHOD5(ShowTranslateUI,
               bool(translate::TranslateStep,
                    const std::string&,
                    const std::string&,
                    TranslateErrors::Type,
                    bool));
  MOCK_METHOD1(IsTranslatableURL, bool(const GURL&));
  MOCK_METHOD1(ShowReportLanguageDetectionErrorUI, void(const GURL&));

 private:
  TranslateDriver* driver_;
  PrefService* prefs_;
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_CLIENT_H_
