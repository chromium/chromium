// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_edit_controller.h"

TestLocationBarModel* TestOmniboxEditController::GetLocationBarModel() {
  return &location_bar_model_;
}

const TestLocationBarModel* TestOmniboxEditController::GetLocationBarModel()
    const {
  return &location_bar_model_;
}

void TestOmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  OmniboxEditController::OnAutocompleteAccept(
      destination_url, post_content, disposition, transition, match_type,
      match_selection_timestamp, destination_url_entered_without_scheme, text,
      match, alternative_nav_match, deviation_char_in_hostname);

  alternate_nav_match_ = alternative_nav_match;
  disposition_ = disposition;
}
