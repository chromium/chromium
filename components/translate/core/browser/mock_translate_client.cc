// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/chromeos_buildflags.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/translate_prefs.h"

namespace translate {

namespace testing {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char* preferred_languages_prefs = "settings.language.preferred_languages";
#else
const char* preferred_languages_prefs = nullptr;
#endif
const char* accept_languages_prefs = "intl.accept_languages";

MockTranslateClient::MockTranslateClient(TranslateDriver* driver,
                                         PrefService* prefs)
    : driver_(driver), prefs_(prefs) {}

MockTranslateClient::~MockTranslateClient() {}

TranslateDriver* MockTranslateClient::GetTranslateDriver() {
  return driver_;
}

PrefService* MockTranslateClient::GetPrefs() {
  return prefs_;
}

std::unique_ptr<TranslatePrefs> MockTranslateClient::GetTranslatePrefs() {
  return std::make_unique<TranslatePrefs>(prefs_);
}

}  // namespace testing
}  // namespace translate
