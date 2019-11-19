// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_record_page_language.h"

#include "content/public/browser/navigation_entry.h"

namespace translate {

namespace {
// The key used to store page language in the NavigationEntry;
const char kPageLanguageKey[] = "page_language";

struct LanguageDetectionData : public base::SupportsUserData::Data {
  // The adopted page language. An ISO 639 language code (two letters, except
  // for Chinese where a localization is necessary).
  std::string page_language;
};
}  // namespace

std::string GetPageLanguageFromNavigation(content::NavigationEntry* entry) {
  auto* data =
      static_cast<LanguageDetectionData*>(entry->GetUserData(kPageLanguageKey));
  return data ? data->page_language : "";
}

void SetPageLanguageInNavigation(const std::string& page_language,
                                 content::NavigationEntry* entry) {
  auto data = std::make_unique<LanguageDetectionData>();
  data->page_language = page_language;
  entry->SetUserData(kPageLanguageKey, std::move(data));
}

}  // namespace translate
