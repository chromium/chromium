// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_CLIENT_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
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

  MOCK_METHOD0(GetAcceptLanguagesService, language::AcceptLanguagesService*());
  MOCK_CONST_METHOD0(GetInfobarIconID, int());

#if BUILDFLAG(IS_IOS)
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<TranslateInfoBarDelegate> delegate) const override {
    return std::make_unique<infobars::InfoBar>(std::move(delegate));
  }
#endif

  MOCK_METHOD5(ShowTranslateUI,
               bool(translate::TranslateStep,
                    const std::string&,
                    const std::string&,
                    TranslateErrors,
                    bool));
  MOCK_METHOD1(IsTranslatableURL, bool(const GURL&));

 private:
  raw_ptr<TranslateDriver> driver_;
  raw_ptr<PrefService> prefs_;
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_CLIENT_H_
