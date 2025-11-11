// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/safety_checks.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_starter_pack_data.h"

class OmniboxClient;
class OmniboxView;

// This class controls the various services that can modify the content of the
// omnibox, including `AutocompleteController` and `OmniboxEditModel`.
class OmniboxController : public AutocompleteController::Observer {
  // TODO(crbug.com/392015004): Remove this macro once it gets fixed.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  explicit OmniboxController(
      std::unique_ptr<OmniboxClient> client,
      std::optional<base::TimeDelta> autocomplete_stop_timer_duration =
          std::nullopt);
  ~OmniboxController() override;
  OmniboxController(const OmniboxController&) = delete;
  OmniboxController& operator=(const OmniboxController&) = delete;

  // Sets the view and enables autocomplete controller observation.
  void SetView(OmniboxView* view);

  // The |current_url| field of input is only set for mobile ports.
  void StartAutocomplete(const AutocompleteInput& input) const;

  // Cancels any pending asynchronous query. If `clear_result` is true, will
  // also erase the result set.
  void StopAutocomplete(bool clear_result) const;

  // Starts an autocomplete prefetch request so that zero-prefix providers can
  // optionally start a prefetch request to warm up the their underlying
  // service(s) and/or optionally cache their otherwise async response.
  void StartZeroSuggestPrefetch();

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  OmniboxClient* client() { return client_.get(); }
  const OmniboxClient* client() const { return client_.get(); }

  OmniboxEditModel* edit_model() { return edit_model_.get(); }
  const OmniboxEditModel* edit_model() const { return edit_model_.get(); }

  void SetEditModelForTesting(std::unique_ptr<OmniboxEditModel> edit_model) {
    edit_model_ = std::move(edit_model);
  }

  AutocompleteController* autocomplete_controller() {
    return autocomplete_controller_.get();
  }

  const AutocompleteController* autocomplete_controller() const {
    return autocomplete_controller_.get();
  }

  void SetAutocompleteControllerForTesting(
      std::unique_ptr<AutocompleteController> autocomplete_controller) {
    autocomplete_controller_ = std::move(autocomplete_controller);
  }

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  // Returns whether or not the row for a particular match should be hidden in
  // the UI. This is currently used to hide suggestions in the 'Gemini' scope
  // when the starter pack expansion feature is enabled.
  bool IsSuggestionHidden(const AutocompleteMatch& match) const;

  // Returns whether any popup is currently open.
  bool IsPopupOpen() const;

  // Sets a callback to validate popup state is in sync with widget visibility.
  // TODO(crbug.com/40251974): Remove this once state manager is proven
  //  reliable.
  void SetPopupStateValidationCallback(
      base::RepeatingCallback<void(OmniboxPopupState)> callback);

  OmniboxPopupStateManager* popup_state_manager() {
    return popup_state_manager_.get();
  }

  const OmniboxPopupStateManager* popup_state_manager() const {
    return popup_state_manager_.get();
  }

 private:
  // Stores the bitmap, using `icon_url` as the key in
  // `edit_model_->icon_bitmaps_` if provided, or `result_index` in
  // `edit_model_->rich_suggestion_bitmaps_` otherwise.
  void SetRichSuggestionBitmap(int result_index,
                               const GURL& icon_url,
                               const SkBitmap& bitmap);

  const std::unique_ptr<OmniboxClient> client_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // `edit_model_` may indirectly contains raw pointers (e.g.
  // `edit_model_->current_match_->provider`) into `AutocompleteProvider`
  // objects owned by `autocomplete_controller_`.  Because of this (per
  // docs/dangling_ptr_guide.md) the `edit_model_` field needs to be declared
  // *after* the `autocomplete_controller_` field.
  std::unique_ptr<OmniboxEditModel> edit_model_;

  // Manages the visibility state of omnibox popups, i.e., None, Classic, AIM.
  std::unique_ptr<OmniboxPopupStateManager> popup_state_manager_;

  // Callback to validate popup state is in sync with widget visibility.
  base::RepeatingCallback<void(OmniboxPopupState)>
      popup_state_validation_callback_;

  base::WeakPtrFactory<OmniboxController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_H_
