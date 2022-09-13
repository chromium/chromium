// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_PAGE_TRANSLATED_DETAILS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_PAGE_TRANSLATED_DETAILS_H_

#include <string>

#include "components/translate/core/common/translate_errors.h"

namespace translate {

// Used when sending a notification about a page that has been translated.
struct PageTranslatedDetails {
  std::string source_language;
  std::string target_language;
  TranslateErrors error_type;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_PAGE_TRANSLATED_DETAILS_H_
