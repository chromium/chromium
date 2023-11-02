// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_model.h"

TestOmniboxEditModel::TestOmniboxEditModel(OmniboxView* view,
                                           OmniboxEditController* controller,
                                           PrefService* pref_service)
    : OmniboxEditModel(view, controller, std::make_unique<TestOmniboxClient>()),
      popup_is_open_(false),
      pref_service_(pref_service) {}

TestOmniboxEditModel::~TestOmniboxEditModel() {}

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
    bool is_keyword_hint,
    const std::u16string& additional_text) {
  OmniboxEditModel::OnPopupDataChanged(
      temporary_text, is_temporary_text, inline_autocompletion,
      prefix_autocompletion, keyword, is_keyword_hint, additional_text);
  text_ = is_temporary_text ? temporary_text : inline_autocompletion;
  is_temporary_text_ = is_temporary_text;
}

PrefService* TestOmniboxEditModel::GetPrefService() const {
  return pref_service_ == nullptr ? OmniboxEditModel::GetPrefService()
                                  : pref_service_.get();
}
