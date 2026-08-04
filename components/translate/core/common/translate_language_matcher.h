// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_LANGUAGE_MATCHER_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_LANGUAGE_MATCHER_H_

#include <string>

#include "base/containers/span.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"

namespace translate {

// Returns the list of default supported languages as LanguageTag elements.
base::span<const base::i18n::LanguageTag> GetDefaultSupportedLanguages();

// Returns the LanguageTagMatcher initialized with Translate default supported
// languages.
const base::i18n::LanguageTagMatcherWithDefault& GetTranslateLanguageMatcher();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_LANGUAGE_MATCHER_H_
