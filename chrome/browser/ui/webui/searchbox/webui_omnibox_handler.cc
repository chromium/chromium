// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"

#include <memory>
#include <utility>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_omnibox_client.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/lens/lens_features.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/cookies/cookie_util.h"
#include "searchbox_handler.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {

searchbox::mojom::SelectionLineState ConvertLineState(
    OmniboxPopupSelection::LineState state) {
  switch (state) {
    case OmniboxPopupSelection::LineState::NORMAL:
      return searchbox::mojom::SelectionLineState::kNormal;
    case OmniboxPopupSelection::LineState::KEYWORD_MODE:
      return searchbox::mojom::SelectionLineState::kKeywordMode;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_AIM:
      // WebUi currently only cares about popup matches' selection states. No
      // need to pipe in a selection state that's unique to the omnibox input.
      return searchbox::mojom::SelectionLineState::kNormal;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION:
      return searchbox::mojom::SelectionLineState::kFocusedButtonAction;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      return searchbox::mojom::SelectionLineState::
          kFocusedButtonRemoveSuggestion;
    default:
      // Realbox doesn't support the other UIs and their focus states.
      NOTREACHED() << state;
  }
}

}  // namespace

WebuiOmniboxHandler::WebuiOmniboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter,
    OmniboxController* omnibox_controller)
    : SearchboxHandler(std::move(pending_page_handler),
                       profile,
                       web_contents,
                       metrics_reporter,
                       /*controller=*/nullptr) {
  // Keep a reference to the OmniboxController instance owned by the
  // `OmniboxView`.
  CHECK(omnibox_controller);
  controller_ = omnibox_controller;
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

WebuiOmniboxHandler::~WebuiOmniboxHandler() = default;

void WebuiOmniboxHandler::AddObserver(
    OmniboxWebuiPopupChangeObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnPopupElementSizeChanged(webui_size_);
}

void WebuiOmniboxHandler::RemoveObserver(
    OmniboxWebuiPopupChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WebuiOmniboxHandler::HasObserver(
    const OmniboxWebuiPopupChangeObserver* observer) const {
  return observers_.HasObserver(observer);
}

void WebuiOmniboxHandler::PopupElementSizeChanged(const gfx::Size& size) {
  webui_size_ = size;
  for (OmniboxWebuiPopupChangeObserver& observer : observers_) {
    observer.OnPopupElementSizeChanged(size);
  }
}

void WebuiOmniboxHandler::UpdateSelection(OmniboxPopupSelection old_selection,
                                          OmniboxPopupSelection selection) {
  page_->UpdateSelection(
      searchbox::mojom::OmniboxPopupSelection::New(
          old_selection.line, ConvertLineState(old_selection.state),
          old_selection.action_index),
      searchbox::mojom::OmniboxPopupSelection::New(
          selection.line, ConvertLineState(selection.state),
          selection.action_index));
}
