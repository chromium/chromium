// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"

#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"

SearchEngineChoiceHandler::SearchEngineChoiceHandler(
    mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver,
    base::OnceCallback<void()> display_dialog_callback,
    base::OnceCallback<void(int, bool)> handle_choice_selected_callback,
    base::RepeatingCallback<void()> handle_learn_more_clicked_callback,
    base::OnceCallback<void()> handle_more_button_clicked_callback)
    : receiver_(this, std::move(receiver)),
      display_dialog_callback_(std::move(display_dialog_callback)),
      handle_choice_selected_callback_(
          std::move(handle_choice_selected_callback)),
      handle_learn_more_clicked_callback_(handle_learn_more_clicked_callback),
      handle_more_button_clicked_callback_(
          std::move(handle_more_button_clicked_callback)) {
  CHECK(handle_choice_selected_callback_);
  CHECK(handle_learn_more_clicked_callback_);
  CHECK(handle_more_button_clicked_callback_);
}

SearchEngineChoiceHandler::~SearchEngineChoiceHandler() = default;

void SearchEngineChoiceHandler::DisplayDialog() {
  if (display_dialog_callback_) {
    std::move(display_dialog_callback_).Run();
  }
}

void SearchEngineChoiceHandler::HandleSearchEngineChoiceSelected(
    int prepopulate_id,
    bool save_guest_mode_selection) {
  if (handle_choice_selected_callback_) {
    std::move(handle_choice_selected_callback_)
        .Run(prepopulate_id, save_guest_mode_selection);
  }
}

void SearchEngineChoiceHandler::HandleLearnMoreLinkClicked() {
  handle_learn_more_clicked_callback_.Run();
}

void SearchEngineChoiceHandler::HandleMoreButtonClicked() {
  if (handle_more_button_clicked_callback_) {
    std::move(handle_more_button_clicked_callback_).Run();
  }
}

void SearchEngineChoiceHandler::RecordScrollState(
    search_engine_choice::mojom::PageHandler_ScrollState scroll_state) {
  base::UmaHistogramEnumeration(
      "Search.ChoiceDebug.ScrollStateWithMoreButtonClick", scroll_state);
}
