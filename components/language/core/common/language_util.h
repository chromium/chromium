// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_

#include <string>

namespace language {

// Some languages like Norwegian and Filipino use different codes within Chrome
// and the Translate service (ie "nb" vs "no" and "fil" vs "tl").
// This converts a Chrome language code to the Translate server synonym. The
// only translate language codes with a country extension are zh-TW and zh-CN,
// the country code is striped from all other languages. Does not check if the
// base language is translatable. Please keep consistent with the same logic in:
// chrome/browser/resources/settings/languages_page/languages.js,
void ToTranslateLanguageSynonym(std::string* language);

// Converts language code synonym to use at Chrome internal.
void ToChromeLanguageSynonym(std::string* language);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_
