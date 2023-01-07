// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_CONTROLLER_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "ui/base/window_open_disposition.h"

class TestOmniboxEditController : public OmniboxEditController {
 public:
  TestOmniboxEditController() {}
  TestOmniboxEditController(const TestOmniboxEditController&) = delete;
  TestOmniboxEditController& operator=(const TestOmniboxEditController&) =
      delete;

  // OmniboxEditController:
  TestLocationBarModel* GetLocationBarModel() override;
  const TestLocationBarModel* GetLocationBarModel() const override;
  void OnAutocompleteAccept(
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
      IDNA2008DeviationCharacter deviation_char_in_hostname =
          IDNA2008DeviationCharacter::kNone) override;

  const AutocompleteMatch& alternate_nav_match() const {
    return alternate_nav_match_;
  }

  const WindowOpenDisposition& disposition() const { return disposition_; }

  using OmniboxEditController::destination_url;

 private:
  TestLocationBarModel location_bar_model_;
  AutocompleteMatch alternate_nav_match_;
  WindowOpenDisposition disposition_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_CONTROLLER_H_
