// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_EDIT_MODEL_H_
#define CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_EDIT_MODEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class TestOmniboxEditModel : public OmniboxEditModel {
 public:
  TestOmniboxEditModel(OmniboxController* omnibox_controller,
                       PrefService* pref_service);
  ~TestOmniboxEditModel() override;
  TestOmniboxEditModel(const TestOmniboxEditModel&) = delete;
  TestOmniboxEditModel& operator=(const TestOmniboxEditModel&) = delete;

  // OmniboxEditModel:
  AutocompleteMatch CurrentMatchAndAlternateNavUrl(
      GURL* alternate_nav_url) const override;
  void OnPopupDataChanged(const std::u16string& temporary_text,
                          bool is_temporary_text,
                          const std::u16string& inline_autocompletion,
                          const std::u16string& keyword,
                          const std::u16string& keyword_placeholder,
                          KeywordState keyword_state,
                          const std::u16string& additional_text,
                          const AutocompleteMatch& match) override;

  // This calls `OpenMatch` directly for the few remaining `OmniboxEditModel`
  // test cases that require explicit control over match content. For new
  // tests, and for non-test code, use `OpenSelection`.
  void OpenMatchForTesting(
      AutocompleteMatch match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t index,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Lookup the bitmap for the first `match` in
  // `autocomplete_controller()->result()` that has `keyword` as its
  // `associated_keyword`. Used to fetch bitmap where the `result_index` is
  // unknown.  Returns nullptr if not found.
  const SkBitmap* GetPopupRichSuggestionBitmapForKeyword(
      const std::u16string& keyword) const;

  void SetCurrentMatchForTest(const AutocompleteMatch& match);

  void NavigateToAiModeWithContextualizerOnContextualizationCompleteForTesting(
      const std::u16string& query_text,
      WindowOpenDisposition disposition,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  bool HasTemporaryText() { return has_temporary_text_; }

  const std::u16string& text() const { return text_; }
  bool is_temporary_text() const { return is_temporary_text_; }

 protected:
  PrefService* GetPrefService() override;
  const PrefService* GetPrefService() const override;

 private:
  std::unique_ptr<AutocompleteMatch> override_current_match_;

  // Contains the most recent text passed by the popup model to the edit model.
  std::u16string text_;
  bool is_temporary_text_ = false;
  raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_EDIT_MODEL_H_
