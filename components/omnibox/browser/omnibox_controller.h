// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"
#include "components/omnibox/browser/autocomplete_match.h"

struct AutocompleteMatch;
class AutocompleteResult;
class InstantController;
class OmniboxClient;
class OmniboxEditModel;
class OmniboxPopupModel;

// This class controls the various services that can modify the content
// for the omnibox, including AutocompleteController and InstantController. It
// is responsible of updating the omnibox content.
// TODO(beaudoin): Keep on expanding this class so that OmniboxEditModel no
//     longer needs to hold any reference to AutocompleteController. Also make
//     this the point of contact between InstantController and OmniboxEditModel.
//     As the refactor progresses, keep the class comment up to date to
//     precisely explain what this class is doing.
class OmniboxController : public AutocompleteControllerDelegate {
 public:
  OmniboxController(OmniboxEditModel* omnibox_edit_model,
                    OmniboxClient* client);
  ~OmniboxController() override;

  // The |current_url| field of input is only set for mobile ports.
  void StartAutocomplete(const AutocompleteInput& input) const;

  // AutocompleteControllerDelegate:
  void OnResultChanged(bool default_match_changed) override;

  AutocompleteController* autocomplete_controller() {
    return autocomplete_controller_.get();
  }

  // Set |current_match_| to an invalid value, indicating that we do not yet
  // have a valid match for the current text in the omnibox.
  void InvalidateCurrentMatch();

  void set_popup_model(OmniboxPopupModel* popup_model) {
    popup_ = popup_model;
  }

  // TODO(beaudoin): The edit and popup model should be siblings owned by the
  // LocationBarView, making this accessor unnecessary.
  OmniboxPopupModel* popup_model() const { return popup_; }

  const AutocompleteMatch& current_match() const { return current_match_; }

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  const AutocompleteResult& result() const {
    return autocomplete_controller_->result();
  }

 private:
  // Stores the bitmap in the OmniboxPopupModel.
  void SetRichSuggestionBitmap(int result_index, const SkBitmap& bitmap);

  // Weak, it owns us.
  // TODO(beaudoin): Consider defining a delegate to ease unit testing.
  OmniboxEditModel* omnibox_edit_model_;

  OmniboxClient* client_;

  OmniboxPopupModel* popup_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // TODO(beaudoin): This AutocompleteMatch is used to let the OmniboxEditModel
  // know what it should display. Not every field is required for that purpose,
  // but the ones specifically needed are unclear. We should therefore spend
  // some time to extract these fields and use a tighter structure here.
  AutocompleteMatch current_match_;

  base::WeakPtrFactory<OmniboxController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OmniboxController);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_
