// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"

class AutocompleteResult;
class OmniboxClient;
class OmniboxEditModel;
class OmniboxEditModelDelegate;
class OmniboxView;
struct AutocompleteMatch;

// This class controls the various services that can modify the content of the
// omnibox, including `AutocompleteController` and `OmniboxEditModel`.
class OmniboxController : public AutocompleteController::Observer {
 public:
  OmniboxController(OmniboxView* view,
                    OmniboxEditModelDelegate* edit_model_delegate,
                    std::unique_ptr<OmniboxClient> client);
  ~OmniboxController() override;
  OmniboxController(const OmniboxController&) = delete;
  OmniboxController& operator=(const OmniboxController&) = delete;

  // The |current_url| field of input is only set for mobile ports.
  void StartAutocomplete(const AutocompleteInput& input) const;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  OmniboxClient* client() { return client_.get(); }

  OmniboxEditModel* edit_model() const { return edit_model_.get(); }
  void set_edit_model(std::unique_ptr<OmniboxEditModel> edit_model);

  AutocompleteController* autocomplete_controller() {
    return autocomplete_controller_.get();
  }
  void set_autocomplete_controller(
      std::unique_ptr<AutocompleteController> autocomplete_controller) {
    autocomplete_controller_ = std::move(autocomplete_controller);
  }

  // Set |current_match_| to an invalid value, indicating that we do not yet
  // have a valid match for the current text in the omnibox.
  void InvalidateCurrentMatch();

  const AutocompleteMatch& current_match() const { return current_match_; }

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  const AutocompleteResult& result() const {
    return autocomplete_controller_->result();
  }

 private:
  // Stores the bitmap in the OmniboxPopupModel.
  void SetRichSuggestionBitmap(int result_index, const SkBitmap& bitmap);

  std::unique_ptr<OmniboxClient> client_;

  std::unique_ptr<OmniboxEditModel> edit_model_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // TODO(beaudoin): This AutocompleteMatch is used to let the OmniboxEditModel
  // know what it should display. Not every field is required for that purpose,
  // but the ones specifically needed are unclear. We should therefore spend
  // some time to extract these fields and use a tighter structure here.
  // TODO(manukh): When `kRedoCurrentMatch` is enabled, this is unused and
  //   replaced by `OmniboxEditModel::current_match_` which serves the same
  //   purpose but is hopefully more often correctly set (`current_match_` here
  //   is almost always invalid).
  AutocompleteMatch current_match_;

  base::WeakPtrFactory<OmniboxController> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_
