// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
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
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {
class RealboxOmniboxClient final : public SearchboxOmniboxClient {
 public:
  RealboxOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~RealboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  void OnBookmarkLaunched() override;
};

RealboxOmniboxClient::RealboxOmniboxClient(Profile* profile,
                                           content::WebContents* web_contents)
    : SearchboxOmniboxClient(profile, web_contents) {}

RealboxOmniboxClient::~RealboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
RealboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::NTP_REALBOX;
}

void RealboxOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BookmarkLaunchLocation::kOmnibox,
                       profile_metrics::GetBrowserProfileType(profile_));
}

}  // namespace

RealboxHandler::RealboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter,
    OmniboxController* omnibox_controller)
    : SearchboxHandler(std::move(pending_page_handler),
                       profile,
                       web_contents,
                       metrics_reporter) {
  // Keep a reference to the OmniboxController instance owned by the OmniboxView
  // when the handler is being used in the context of the omnibox popup.
  // Otherwise, create own instance of OmniboxController. Either way, observe
  // the AutocompleteController instance owned by the OmniboxController.
  if (omnibox_controller) {
    controller_ = omnibox_controller;
  } else {
    owned_controller_ = std::make_unique<OmniboxController>(
        /*view=*/nullptr,
        std::make_unique<RealboxOmniboxClient>(profile_, web_contents_),
        kAutocompleteDefaultStopTimerDuration);
    controller_ = owned_controller_.get();
  }

  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

RealboxHandler::~RealboxHandler() = default;

void RealboxHandler::AddObserver(OmniboxWebUIPopupChangeObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnPopupElementSizeChanged(webui_size_);
}

void RealboxHandler::RemoveObserver(OmniboxWebUIPopupChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool RealboxHandler::HasObserver(
    const OmniboxWebUIPopupChangeObserver* observer) const {
  return observers_.HasObserver(observer);
}

void RealboxHandler::PopupElementSizeChanged(const gfx::Size& size) {
  webui_size_ = size;
  for (OmniboxWebUIPopupChangeObserver& observer : observers_) {
    observer.OnPopupElementSizeChanged(size);
  }
}

void RealboxHandler::DeleteAutocompleteMatch(uint8_t line, const GURL& url) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match || !match->SupportsDeletion()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  omnibox_controller()->StopAutocomplete(/*clear_result=*/false);
  autocomplete_controller()->DeleteMatch(*match);
}

void RealboxHandler::ExecuteAction(uint8_t line,
                                   uint8_t action_index,
                                   const GURL& url,
                                   base::TimeTicks match_selection_timestamp,
                                   uint8_t mouse_button,
                                   bool alt_key,
                                   bool ctrl_key,
                                   bool meta_key,
                                   bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  if (action_index >= match->actions.size()) {
    return;
  }
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  OmniboxPopupSelection selection(
      line, OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION,
      action_index);
  edit_model()->OpenSelection(selection, match_selection_timestamp,
                              disposition);
}

searchbox::mojom::SelectionLineState ConvertLineState(
    OmniboxPopupSelection::LineState state) {
  switch (state) {
    case OmniboxPopupSelection::LineState::NORMAL:
      return searchbox::mojom::SelectionLineState::kNormal;
    case OmniboxPopupSelection::LineState::KEYWORD_MODE:
      return searchbox::mojom::SelectionLineState::kKeywordMode;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION:
      return searchbox::mojom::SelectionLineState::kFocusedButtonAction;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      return searchbox::mojom::SelectionLineState::
          kFocusedButtonRemoveSuggestion;
    default:
      NOTREACHED();
  }
}

void RealboxHandler::UpdateSelection(OmniboxPopupSelection old_selection,
                                     OmniboxPopupSelection selection) {
  page_->UpdateSelection(
      searchbox::mojom::OmniboxPopupSelection::New(
          old_selection.line, ConvertLineState(old_selection.state),
          old_selection.action_index),
      searchbox::mojom::OmniboxPopupSelection::New(
          selection.line, ConvertLineState(selection.state),
          selection.action_index));
}
