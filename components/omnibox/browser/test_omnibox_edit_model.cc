// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_model.h"

TestOmniboxEditModel::TestOmniboxEditModel(OmniboxView* view,
                                           OmniboxEditController* controller)
    : OmniboxEditModel(view, controller, std::make_unique<TestOmniboxClient>()),
      popup_is_open_(false) {}

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
