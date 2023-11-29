// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/strings/grit/search_engine_descriptions_strings.h"

namespace search_engines {
// This file will be generated. Don't modify it manually.
int GetMarketingSnippetResourceId(const std::u16string& engine_keyword) {
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::google.keyword)) {
    return IDS_GOOGLE_SEARCH_DESCRIPTION;
  }
  return -1;
}
}  // namespace search_engines
