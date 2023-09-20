// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/prefs/pref_change_registrar.h"

class AutocompleteResult;
class OmniboxClient;
class OmniboxView;

// This class controls the various services that can modify the content of the
// omnibox, including `AutocompleteController` and `OmniboxEditModel`.
class OmniboxController : public AutocompleteController::Observer {
 public:
  OmniboxController(OmniboxView* view, std::unique_ptr<OmniboxClient> client);
  ~OmniboxController() override;
  OmniboxController(const OmniboxController&) = delete;
  OmniboxController& operator=(const OmniboxController&) = delete;

  // The |current_url| field of input is only set for mobile ports.
  void StartAutocomplete(const AutocompleteInput& input) const;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  OmniboxClient* client() { return client_.get(); }

  OmniboxEditModel* edit_model() { return edit_model_.get(); }

  void SetEditModelForTesting(std::unique_ptr<OmniboxEditModel> edit_model) {
    edit_model_ = std::move(edit_model);
  }

  AutocompleteController* autocomplete_controller() {
    return autocomplete_controller_.get();
  }

  void SetAutocompleteControllerForTesting(
      std::unique_ptr<AutocompleteController> autocomplete_controller) {
    autocomplete_controller_ = std::move(autocomplete_controller);
  }

  const AutocompleteResult& result() const {
    return autocomplete_controller_->result();
  }

  // Returns whether `AutocompleteController` is currently processing a query.
  bool query_in_progress() const { return !autocomplete_controller_->done(); }

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  // Returns the header string associated with `suggestion_group_id`, or an
  // empty string if `suggestion_group_id` is not found in the results.
  std::u16string GetHeaderForSuggestionGroup(
      omnibox::GroupId suggestion_group_id) const;

  // Returns whether or not `suggestion_group_id` should be collapsed in the UI.
  // This method takes into account both the user's stored prefs as well as
  // the server-provided visibility hint. Returns false if `suggestion_group_id`
  // is not found in the results.
  bool IsSuggestionGroupHidden(omnibox::GroupId suggestion_group_id) const;

  // Sets the UI collapsed/expanded state of the `suggestion_group_id` in the
  // user's stored prefs based on the value of `hidden`. Does nothing if
  // `suggestion_group_id` is not found in the results.
  void SetSuggestionGroupHidden(omnibox::GroupId suggestion_group_id,
                                bool hidden) const;

 private:
  // Stores the bitmap in the OmniboxPopupModel.
  void SetRichSuggestionBitmap(int result_index, const SkBitmap& bitmap);

  // Called when the prefs for the visibility of groups changes.
  void OnSuggestionGroupVisibilityPrefChanged();

  std::unique_ptr<OmniboxClient> client_;

  std::unique_ptr<OmniboxEditModel> edit_model_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // Observes changes to the prefs for the visibility of groups.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<OmniboxController> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_H_
