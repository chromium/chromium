// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "ui/gfx/geometry/rect.h"

OmniboxController::OmniboxController(
    std::unique_ptr<OmniboxClient> client,
    std::optional<base::TimeDelta> autocomplete_stop_timer_duration)
    : client_(std::move(client)),
      edit_model_(std::make_unique<OmniboxEditModel>(
          /*omnibox_controller=*/this)),
      popup_state_manager_(std::make_unique<OmniboxPopupStateManager>()) {
  AutocompleteControllerConfig autocomplete_controller_config{
      .provider_types = AutocompleteClassifier::DefaultOmniboxProviders()};
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    autocomplete_controller_config.show_iph_matches = false;
  }
  if (autocomplete_stop_timer_duration.has_value()) {
    autocomplete_controller_config.stop_timer_duration =
        autocomplete_stop_timer_duration.value();
  }
  autocomplete_controller_ = std::make_unique<AutocompleteController>(
      client_->CreateAutocompleteProviderClient(),
      autocomplete_controller_config);

  // Register the `AutocompleteController` with `AutocompleteControllerEmitter`.
  if (auto* emitter = client_->GetAutocompleteControllerEmitter()) {
    autocomplete_controller_->AddObserver(emitter);
  }
}

void OmniboxController::SetView(OmniboxView* view) {
  edit_model_->set_view(view);
  if (view) {
    // Start observing the AutocompleteController when a View is associated with
    // the OmniboxController. WebUI searchboxes observe the
    // AutocompleteController directly in the WebUI page handler.
    autocomplete_controller_->AddObserver(this);
  }
}

constexpr bool is_ios = !!BUILDFLAG(IS_IOS);

OmniboxController::~OmniboxController() = default;

void OmniboxController::StartAutocomplete(
    const AutocompleteInput& input) const {
  TRACE_EVENT0("omnibox", "OmniboxController::StartAutocomplete");
  ClearPopupKeywordMode();

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(input);
}

void OmniboxController::StopAutocomplete(bool clear_result) const {
  TRACE_EVENT0("omnibox", "OmniboxController::StopAutocomplete");
  autocomplete_controller_->Stop(clear_result
                                     ? AutocompleteStopReason::kClobbered
                                     : AutocompleteStopReason::kInteraction);
}

void OmniboxController::StartZeroSuggestPrefetch() {
  TRACE_EVENT0("omnibox", "OmniboxController::StartZeroSuggestPrefetch");
  client_->MaybePrewarmForDefaultSearchEngine();

  auto page_classification =
      client_->GetPageClassification(/*is_prefetch=*/true);

  GURL current_url = client_->GetURL();
  std::u16string text = base::UTF8ToUTF16(current_url.spec());

  if (omnibox::IsNTPPage(page_classification) || !is_ios) {
    text.clear();
  }

  AutocompleteInput input(text, page_classification,
                          client_->GetSchemeClassifier());
  input.set_current_url(current_url);
  input.set_current_title(client_->GetTitle());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  autocomplete_controller_->StartPrefetch(input);
}

void OmniboxController::OnResultChanged(AutocompleteController* controller,
                                        bool default_match_changed) {
  TRACE_EVENT0("omnibox", "OmniboxController::OnResultChanged");
  DCHECK(controller == autocomplete_controller_.get());
  const bool popup_was_open = IsPopupOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModel know
    // about new inline autocomplete text (blue highlight).
    if (autocomplete_controller_->result().default_match()) {
      edit_model_->OnCurrentMatchChanged();
    } else {
      edit_model_->OnPopupResultChanged();
      edit_model_->OnPopupDataChanged(
          std::u16string(),
          /*is_temporary_text=*/false, std::u16string(), std::u16string(),
          std::u16string(), false, std::u16string(), AutocompleteMatch());
    }
  } else {
    edit_model_->OnPopupResultChanged();
  }

  const bool popup_is_open = IsPopupOpen();
  if (popup_was_open != popup_is_open) {
    client_->OnPopupVisibilityChanged(popup_is_open);
  }

  if (popup_was_open && !popup_is_open) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    edit_model_->AcceptTemporaryTextAsUserText();
    // Closing the popup can change the default suggestion. This usually occurs
    // when it's unclear whether the input represents a search or URL; e.g.,
    // 'a.com/b c' or when title autocompleting. Clear the additional text to
    // avoid suggesting the omnibox contains a URL suggestion when that may no
    // longer be the case; i.e. when the default suggestion changed from a URL
    // to a search suggestion upon closing the popup.
    edit_model_->ClearAdditionalText();
  }

  // Note: The client outlives |this|, so bind a weak pointer to the callback
  // passed in to eliminate the potential for crashes on shutdown.
  // `should_preload` is set to `controller->done()` as prerender may only want
  // to start preloading a result after all Autocomplete results are ready.
  client_->OnResultChanged(
      autocomplete_controller_->result(), default_match_changed,
      /*should_preload=*/controller->done(),
      base::BindRepeating(&OmniboxController::SetRichSuggestionBitmap,
                          weak_ptr_factory_.GetWeakPtr()));
}

void OmniboxController::ClearPopupKeywordMode() const {
  TRACE_EVENT0("omnibox", "OmniboxController::ClearPopupKeywordMode");
  if (IsPopupOpen()) {
    OmniboxPopupSelection selection = edit_model_->GetPopupSelection();
    if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
      selection.state = OmniboxPopupSelection::NORMAL;
      edit_model_->SetPopupSelection(selection);
    }
  }
}

bool OmniboxController::IsSuggestionHidden(
    const AutocompleteMatch& match) const {
  return match.ShouldHideBasedOnStarterPack(client_->GetTemplateURLService());
}

bool OmniboxController::IsPopupOpen() const {
  OmniboxPopupState state = popup_state_manager_->popup_state();
  if (popup_state_validation_callback_) {
    popup_state_validation_callback_.Run(state);
  }
  return state != OmniboxPopupState::kNone;
}

void OmniboxController::SetPopupStateValidationCallback(
    base::RepeatingCallback<void(OmniboxPopupState)> callback) {
  popup_state_validation_callback_ = std::move(callback);
}

void OmniboxController::SetRichSuggestionBitmap(int result_index,
                                                const GURL& icon_url,
                                                const SkBitmap& bitmap) {
  if (!icon_url.is_empty()) {
    edit_model_->SetIconBitmap(icon_url, bitmap);
  } else {
    edit_model_->SetPopupRichSuggestionBitmap(result_index, bitmap);
  }
}
