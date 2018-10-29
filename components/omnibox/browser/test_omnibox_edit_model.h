// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_MODEL_H_

#include <memory>

#include "components/omnibox/browser/omnibox_edit_model.h"

class TestOmniboxEditModel : public OmniboxEditModel {
 public:
  TestOmniboxEditModel(OmniboxView* view, OmniboxEditController* controller);
  ~TestOmniboxEditModel() override;

  // OmniboxEditModel:
  bool PopupIsOpen() const override;
  AutocompleteMatch CurrentMatch(GURL* alternate_nav_url) const override;

  void SetPopupIsOpen(bool open);

  void SetCurrentMatchForTest(const AutocompleteMatch& match);

 private:
  bool popup_is_open_;
  std::unique_ptr<AutocompleteMatch> override_current_match_;

  DISALLOW_COPY_AND_ASSIGN(TestOmniboxEditModel);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_MODEL_H_
