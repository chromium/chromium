// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/lens/lens_features.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/contextual_search_provider.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/l10n/l10n_util.h"
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

WebuiOmniboxHandler::WebContentsObserver::WebContentsObserver(
    WebuiOmniboxHandler* handler,
    content::WebContents* web_contents)
    : handler_(handler) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (browser_window_interface) {
    Observe(
        browser_window_interface->GetTabStripModel()->GetActiveWebContents());
  }
}

void WebuiOmniboxHandler::WebContentsObserver::ScopedObserve(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

void WebuiOmniboxHandler::WebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  handler_->OnNavigationFinished(handle);
}

WebuiOmniboxHandler::WebuiOmniboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    MetricsReporter* metrics_reporter,
    OmniboxController* omnibox_controller,
    content::WebUI* web_ui,
    GetSessionHandleCallback get_session_callback)
    : ContextualSearchboxHandler(std::move(pending_page_handler),
                                 Profile::FromWebUI(web_ui),
                                 web_ui->GetWebContents(),
                                 /*controller=*/nullptr,
                                 std::move(get_session_callback)),
      web_contents_observer_(/*handler=*/this, web_ui->GetWebContents()),
      metrics_reporter_(metrics_reporter) {
  // Keep a reference to the OmniboxController instance owned by the
  // `OmniboxView`.
  CHECK(omnibox_controller);
  controller_ = omnibox_controller;
  autocomplete_controller_observation_.Observe(autocomplete_controller());
  edit_model_observation_.Observe(omnibox_controller->edit_model());

  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(Profile::FromWebUI(web_ui));
  if (aim_eligibility_service) {
    aim_eligibility_subscription_ =
        aim_eligibility_service->RegisterEligibilityChangedCallback(
            base::BindRepeating(&WebuiOmniboxHandler::OnAimEligibilityChanged,
                                weak_ptr_factory_.GetWeakPtr()));
  }
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      omnibox::kShowAiModeOmniboxButton,
      base::BindRepeating(&WebuiOmniboxHandler::OnShowAiModeButtonPrefChanged,
                          base::Unretained(this)));
}

WebuiOmniboxHandler::~WebuiOmniboxHandler() = default;

// TODO(crbug.com/469098088): Use something other than
//   `AutocompleteController::Observer::OnStart()` to reduce the IPC overhead
//   due to the fact that `AutocompleteController::Start()` gets invoked on
//   *every* keystroke in the Omnibox.
void WebuiOmniboxHandler::OnStart(AutocompleteController* controller,
                                  const AutocompleteInput& input) {
  // Ignore the call until the page remote is bound and ready to receive calls.
  if (!IsRemoteBound()) {
    return;
  }

  const AutocompleteProviderClient* client =
      autocomplete_controller()->autocomplete_provider_client();
  page_->UpdateLensSearchEligibility(
      ContextualSearchProvider::LensEntrypointEligible(input, client) &&
      input.IsZeroSuggest());
}

void WebuiOmniboxHandler::OnResultChanged(AutocompleteController* controller,
                                          bool default_match_changed) {
  const bool ready = IsRemoteBound();
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("FirstAccess")) {
    metrics_reporter_->Mark("FirstAccess");
    base::UmaHistogramBoolean(
        "Omnibox.Popup.WebUI.PageRemoteIsBoundOnFirstCall", ready);
  }

  // Ignore the call until the page remote is bound and ready to receive calls.
  if (!ready) {
    return;
  }

  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("ResultChanged")) {
    metrics_reporter_->Mark("ResultChanged");
  }
  SearchboxHandler::OnResultChanged(controller, default_match_changed);
}

void WebuiOmniboxHandler::OnKeywordStateChanged(bool is_keyword_selected) {
  // Ignore the call until the page remote is bound and ready to receive calls.
  if (!IsRemoteBound()) {
    return;
  }

  page_->SetKeywordSelected(is_keyword_selected);
}

void WebuiOmniboxHandler::OnSelectionChanged(
    OmniboxPopupSelection old_selection,
    OmniboxPopupSelection selection) {
  const bool ready = IsRemoteBound();
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("FirstAccess")) {
    metrics_reporter_->Mark("FirstAccess");
    base::UmaHistogramBoolean("Omnibox.Popup.WebUI.PageIsReadyOnFirstCall",
                              ready);
  }

  // Ignore the call until the page remote is bound and ready to receive calls.
  if (!ready) {
    return;
  }

  page_->UpdateSelection(
      searchbox::mojom::OmniboxPopupSelection::New(
          old_selection.line, ConvertLineState(old_selection.state),
          old_selection.action_index),
      searchbox::mojom::OmniboxPopupSelection::New(
          selection.line, ConvertLineState(selection.state),
          selection.action_index));
}

void WebuiOmniboxHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  web_contents_observer_.ScopedObserve(selection.new_contents);
  ContextualSearchboxHandler::OnTabStripModelChanged(tab_strip_model, change,
                                                     selection);
}

void WebuiOmniboxHandler::OnNavigationFinished(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame()) {
    if (IsRemoteBound()) {
      page_->OnTabStripChanged();
    }
  }
}

void WebuiOmniboxHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  // The rest of this function mirrors
  // `OmniboxSuggestionButtonRowView::ButtonPressed()`.
  OmniboxPopupSelection selection(
      line, OmniboxPopupSelection::LineState::KEYWORD_MODE);
  // Note: Since keyword mode logic depends on state of the edit model, the
  // selection must first be set to prepare for keyword mode before accepting.
  edit_model()->SetPopupSelection(selection);
  // Don't re-enter keyword mode if already in it. This occurs when the user
  // was in keyword mode and re-clicked the same or a different keyword chip.
  if (edit_model()->is_keyword_hint()) {
    const auto entry_method = is_mouse_event
                                  ? metrics::OmniboxEventProto::CLICK_HINT_VIEW
                                  : metrics::OmniboxEventProto::TAP_HINT_VIEW;
    edit_model()->AcceptKeyword(entry_method);
  }
}

void WebuiOmniboxHandler::ShowContextMenu(const gfx::Point& point) {
  if (embedder_) {
    embedder_->ShowContextMenu(point, nullptr);
  }
}

void WebuiOmniboxHandler::OpenLensSearch() {
  edit_model()->OpenLensSearch();
}

void WebuiOmniboxHandler::AddTabContext(int32_t tab_id,
                                        bool delay_upload,
                                        AddTabContextCallback callback) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto tab_strip_id = tab_strip_model->GetIndexOfTab(tab);

  TabRendererData tab_renderer_data =
      TabRendererData::FromTabInModel(tab_strip_model, tab_strip_id);

  SearchboxContextData* searchbox_context_data =
      browser_window_interface->GetFeatures().searchbox_context_data();
  if (!searchbox_context_data) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  auto context = searchbox_context_data->TakePendingContext();
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }

  auto tab_attachment = searchbox::mojom::TabAttachmentStub::New();
  tab_attachment->tab_id = tab_id;
  tab_attachment->title = base::UTF16ToUTF8(tab_renderer_data.title);
  tab_attachment->url = tab_renderer_data.last_committed_url;
  context->file_infos.push_back(
      searchbox::mojom::SearchContextAttachmentStub::NewTabAttachment(
          std::move(tab_attachment)));

  searchbox_context_data->SetPendingContext(std::move(context));

  edit_model()->OpenAiMode(false, /*via_context_menu=*/false);
  std::move(callback).Run(std::nullopt);
}

void WebuiOmniboxHandler::OnShow() {
  // Ignore the call until the page remote is bound and ready to receive calls.
  if (!IsRemoteBound()) {
    return;
  }
  page_->OnShow();
}

void WebuiOmniboxHandler::SetPage(
    mojo::PendingRemote<searchbox::mojom::Page> pending_page) {
  ContextualSearchboxHandler::SetPage(std::move(pending_page));
  OnAimEligibilityChanged();
  OnShowAiModeButtonPrefChanged();
}

void WebuiOmniboxHandler::OnShowAiModeButtonPrefChanged() {
  if (!IsRemoteBound()) {
    return;
  }
  bool show =
      profile_->GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton);
  page_->OnShowAiModePrefChanged(show);
}

std::optional<searchbox::mojom::AutocompleteMatchPtr>
WebuiOmniboxHandler::CreateAutocompleteMatch(
    const AutocompleteMatch& match,
    size_t line,
    const OmniboxEditModel* edit_model,
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map,
    const TemplateURLService* turl_service) const {
  auto mojom_match = SearchboxHandler::CreateAutocompleteMatch(
      match, line, edit_model, bookmark_model, suggestion_groups_map,
      turl_service);

  mojom_match.value()->has_instant_keyword =
      match.HasInstantKeyword(turl_service);
  if (mojom_match && !match.HasInstantKeyword(turl_service) &&
      edit_model->IsPopupControlPresentOnMatch(
          OmniboxPopupSelection{line, OmniboxPopupSelection::KEYWORD_MODE})) {
    const auto names = SelectedKeywordView::GetKeywordLabelNames(
        match.associated_keyword, turl_service);
    mojom_match.value()->keyword_chip_hint = base::UTF16ToUTF8(names.full_name);
    mojom_match.value()->keyword_chip_a11y =
        l10n_util::GetStringFUTF8(IDS_ACC_KEYWORD_MODE, names.short_name);
  }

  return mojom_match;
}

void WebuiOmniboxHandler::OnAimEligibilityChanged() {
  // Ignore the call until the page remote is bound and ready to receive calls.
  if (!IsRemoteBound()) {
    return;
  }

  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);
  if (aim_eligibility_service) {
    bool eligible = aim_eligibility_service->IsAimEligible();
    page_->UpdateAimEligibility(eligible);
  }
}

int WebuiOmniboxHandler::GetContextMenuMaxTabSuggestions() {
  return omnibox::kContextMenuMaxTabSuggestions.Get();
}

std::optional<lens::LensOverlayInvocationSource>
WebuiOmniboxHandler::GetInvocationSource() const {
  return lens::LensOverlayInvocationSource::kOmniboxContextualQuery;
}
