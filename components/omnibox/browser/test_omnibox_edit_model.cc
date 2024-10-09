// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_edit_model.h"

#include <memory>

#include "components/omnibox/browser/test_omnibox_client.h"

TestOmniboxEditModel::TestOmniboxEditModel(
    OmniboxController* omnibox_controller,
    OmniboxView* view,
    PrefService* pref_service)
    : OmniboxEditModel(omnibox_controller, view),
      popup_is_open_(false),
      pref_service_(pref_service) {}

TestOmniboxEditModel::~TestOmniboxEditModel() = default;

bool TestOmniboxEditModel::PopupIsOpen() const {
  return popup_is_open_;
}

AutocompleteMatch TestOmniboxEditModel::CurrentMatch(
    GURL* alternate_nav_url) const {
  if (override_current_match_)
    return *override_current_match_;

  return OmniboxEditModel::CurrentMatch(alternate_nav_url);
}

void TestOmniboxEditModel::SetPopupIsOpen(bool open) {
  popup_is_open_ = open;
}

void TestOmniboxEditModel::SetCurrentMatchForTest(
    const AutocompleteMatch& match) {
  override_current_match_ = std::make_unique<AutocompleteMatch>(match);
}

void TestOmniboxEditModel::OnPopupDataChanged(
    const std::u16string& temporary_text,
    bool is_temporary_text,
    const std::u16string& inline_autocompletion,
    const std::u16string& prefix_autocompletion,
    const std::u16string& keyword,
    const std::u16string& keyword_placeholder,
    bool is_keyword_hint,
    const std::u16string& additional_text,
    const AutocompleteMatch& match) {
  OmniboxEditModel::OnPopupDataChanged(
      temporary_text, is_temporary_text, inline_autocompletion,
      prefix_autocompletion, keyword, keyword_placeholder, is_keyword_hint,
      additional_text, match);
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
