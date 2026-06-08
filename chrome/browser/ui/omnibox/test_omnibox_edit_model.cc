// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/test_omnibox_edit_model.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

TestOmniboxEditModel::TestOmniboxEditModel(
    OmniboxController* omnibox_controller,
    PrefService* pref_service)
    : OmniboxEditModel(omnibox_controller), pref_service_(pref_service) {}

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

void TestOmniboxEditModel::
    NavigateToAiModeWithContextualizerOnContextualizationCompleteForTesting(
        const std::u16string& query_text,
        WindowOpenDisposition disposition,
        base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
            session_handle) {
  NavigateToAiModeWithContextualizerOnContextualizationComplete(
      query_text, disposition, session_handle);
}

void TestOmniboxEditModel::OnPopupDataChanged(
    const std::u16string& temporary_text,
    bool is_temporary_text,
    const std::u16string& inline_autocompletion,
    const std::u16string& keyword,
    const std::u16string& keyword_placeholder,
    KeywordState keyword_state,
    const std::u16string& additional_text,
    const AutocompleteMatch& match) {
  OmniboxEditModel::OnPopupDataChanged(
      temporary_text, is_temporary_text, inline_autocompletion, keyword,
      keyword_placeholder, keyword_state, additional_text, match);
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
