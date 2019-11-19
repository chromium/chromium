// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_

#include <string>

namespace language {

// Converts language code synonym to use at Translate server.
//
// The same logic exists in
// chrome/browser/resources/settings/languages_page/languages.js,
// please keep consistency with the JavaScript file.
void ToTranslateLanguageSynonym(std::string* language);

// Converts language code synonym to use at Chrome internal.
void ToChromeLanguageSynonym(std::string* language);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_
