// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/test_omnibox_edit_model.h"

#include <algorithm>
#include <memory>

#include "components/omnibox/browser/test_omnibox_client.h"

TestOmniboxEditModel::TestOmniboxEditModel(
    OmniboxController* omnibox_controller,
    PrefService* pref_service)
    : OmniboxEditModel(omnibox_controller),
      pref_service_(pref_service) {}

TestOmniboxEditModel::~TestOmniboxEditModel() = default;

AutocompleteMatch TestOmniboxEditModel::CurrentMatchAndAlternateNavUrl(
    GURL* alternate_nav_url) const {
  if (override_current_match_) {
    return *override_current_match_;
  }

  return OmniboxEditModel::CurrentMatchAndAlternateNavUrl(alternate_nav_url);
}

void TestOmniboxEditModel::OpenMatchForTesting(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t index,
    base::TimeTicks match_selection_timestamp) {
  OpenMatch(OmniboxPopupSelection(index), match, disposition, alternate_nav_url,
            pasted_text, match_selection_timestamp);
}

const SkBitmap* TestOmniboxEditModel::GetPopupRichSuggestionBitmapForKeyword(
    const std::u16string& keyword) const {
  const auto& result = autocomplete_controller()->result();
  auto it =
      std::ranges::find_if(result, [&keyword](const AutocompleteMatch& match) {
        return match.associated_keyword == keyword;
      });
  return it == result.end()
             ? nullptr
             : GetPopupRichSuggestionBitmap(std::distance(result.begin(), it));
}

void TestOmniboxEditModel::SetCurrentMatchForTest(
    const AutocompleteMatch& match) {
  override_current_match_ = std::make_unique<AutocompleteMatch>(match);
}

void TestOmniboxEditModel::OnPopupDataChanged(
    const std::u16string& temporary_text,
    bool is_temporary_text,
    const std::u16string& inline_autocompletion,
    const std::u16string& keyword,
    const std::u16string& keyword_placeholder,
    bool is_keyword_hint,
    const std::u16string& additional_text,
    const AutocompleteMatch& match) {
  OmniboxEditModel::OnPopupDataChanged(
      temporary_text, is_temporary_text, inline_autocompletion, keyword,
      keyword_placeholder, is_keyword_hint, additional_text, match);
  text_ = is_temporary_text ? temporary_text : inline_autocompletion;
  is_temporary_text_ = is_temporary_text;
}

PrefService* TestOmniboxEditModel::GetPrefService() {
  return const_cast<PrefService*>(
      const_cast<const TestOmniboxEditModel*>(this)->GetPrefService());
}

const PrefService* TestOmniboxEditModel::GetPrefService() const {
  return pref_service_ == nullptr ? OmniboxEditModel::GetPrefService()
                                  : pref_service_.get();
}
