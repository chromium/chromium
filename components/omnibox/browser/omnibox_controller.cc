// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "ui/gfx/geometry/rect.h"

OmniboxController::OmniboxController(OmniboxEditModel* omnibox_edit_model,
                                     OmniboxClient* client)
    : omnibox_edit_model_(omnibox_edit_model),
      client_(client),
      autocomplete_controller_(new AutocompleteController(
          client_->CreateAutocompleteProviderClient(),
          AutocompleteClassifier::DefaultOmniboxProviders())) {
  autocomplete_controller_->AddObserver(this);

  OmniboxControllerEmitter* emitter = client_->GetOmniboxControllerEmitter();
  if (emitter)
    autocomplete_controller_->AddObserver(emitter);
}

OmniboxController::~OmniboxController() {
}

void OmniboxController::StartAutocomplete(
    const AutocompleteInput& input) const {
  ClearPopupKeywordMode();

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(input);
}

void OmniboxController::OnResultChanged(AutocompleteController* controller,
                                        bool default_match_changed) {
  DCHECK(controller == autocomplete_controller_.get());

  const bool was_open = omnibox_edit_model_->PopupIsOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModel know
    // about new inline autocomplete text (blue highlight).
    if (auto* match = result().default_match()) {
      current_match_ = *match;
      omnibox_edit_model_->OnCurrentMatchChanged();
    } else {
      InvalidateCurrentMatch();
      omnibox_edit_model_->OnPopupResultChanged();
      omnibox_edit_model_->OnPopupDataChanged(
          std::u16string(),
          /*is_temporary_text=*/false, std::u16string(), std::u16string(),
          std::u16string(), false, std::u16string());
    }
  } else {
    omnibox_edit_model_->OnPopupResultChanged();
  }

  if (was_open && !omnibox_edit_model_->PopupIsOpen()) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    omnibox_edit_model_->AcceptTemporaryTextAsUserText();
    // Closing the popup can change the default suggestion. This usually occurs
    // when it's unclear whether the input represents a search or URL; e.g.,
    // 'a.com/b c' or when title autocompleting. Clear the additional text to
    // avoid suggesting the omnibox contains a URL suggestion when that may no
    // longer be the case; i.e. when the default suggestion changed from a URL
    // to a search suggestion upon closing the popup.
    omnibox_edit_model_->ClearAdditionalText();
  }

  // Note: The client outlives |this|, so bind a weak pointer to the callback
  // passed in to eliminate the potential for crashes on shutdown.
  // `should_prerender` is set to `controller->done()` as prerender may only
  // want to start prerendering a result after all Autocomplete results are
  // ready.
  client_->OnResultChanged(
      result(), default_match_changed, /*should_prerender=*/controller->done(),
      base::BindRepeating(&OmniboxController::SetRichSuggestionBitmap,
                          weak_ptr_factory_.GetWeakPtr()));
}

void OmniboxController::InvalidateCurrentMatch() {
  current_match_ = AutocompleteMatch();
}

void OmniboxController::ClearPopupKeywordMode() const {
  if (omnibox_edit_model_->PopupIsOpen()) {
    OmniboxPopupSelection selection = omnibox_edit_model_->GetPopupSelection();
    if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
      selection.state = OmniboxPopupSelection::NORMAL;
      omnibox_edit_model_->SetPopupSelection(selection);
    }
  }
}

void OmniboxController::SetRichSuggestionBitmap(int result_index,
                                                const SkBitmap& bitmap) {
  omnibox_edit_model_->SetPopupRichSuggestionBitmap(result_index, bitmap);
}
