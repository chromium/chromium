// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/pref_names.h"
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
#include "components/omnibox/browser/searchbox.mojom-shared.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
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
      return searchbox::mojom::SelectionLineState::kFocusedButtonAim;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION:
      return searchbox::mojom::SelectionLineState::kFocusedButtonAction;
    case OmniboxPopupSelection::LineState::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      return searchbox::mojom::SelectionLineState::
          kFocusedButtonRemoveSuggestion;
    default:
      // WebUI omnibox doesn't support the other UIs and their focus states.
      NOTREACHED() << state;
  }
}

}  // namespace

WebuiOmniboxHandler::WebuiOmniboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    MetricsReporter* metrics_reporter,
    OmniboxController* omnibox_controller,
    content::WebUI* web_ui,
    GetSessionHandleCallback get_session_callback)
    : ContextualSearchboxHandler(std::move(pending_page_handler),
                                 std::move(pending_page),
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
            base::BindRepeating(&WebuiOmniboxHandler::OnAimPopupEligibilityChanged,
                                base::Unretained(this)));
  }
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      omnibox::kShowAiModeOmniboxButton,
      base::BindRepeating(&WebuiOmniboxHandler::OnAimPopupEligibilityChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      contextual_search::kSearchContentSharingSettings,
      base::BindRepeating(&WebuiOmniboxHandler::OnContentSharingPolicyChanged,
                          base::Unretained(this)));

  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_ui->GetWebContents())) {
    tab_will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &WebuiOmniboxHandler::OnTabWillDetach, base::Unretained(this)));
    tab_did_insert_subscription_ = tab->RegisterDidInsert(base::BindRepeating(
        &WebuiOmniboxHandler::OnTabDidInsert, base::Unretained(this)));
  }

  OnAimPopupEligibilityChanged();
  OnContentSharingPolicyChanged();

  // Ensure the page receives the current autocomplete state on startup.
  // This handles the case where results are generated before the remote is
  // bound and the handler is created and starts observing the
  // AutocompleteController.
  OnResultChanged(controller_->autocomplete_controller(), false);
}

WebuiOmniboxHandler::~WebuiOmniboxHandler() = default;

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

void WebuiOmniboxHandler::OpenLensSearch() {
  edit_model()->OpenLensSearch();
}

void WebuiOmniboxHandler::AddTabContext(int32_t tab_id,
                                        bool delay_upload,
                                        AddTabContextCallback callback) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  SearchboxContextData* searchbox_context_data =
      browser_window_interface->GetFeatures().searchbox_context_data();
  if (!searchbox_context_data) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }
  auto context = searchbox_context_data->TakePendingContext();
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }

  auto tab_attachment = searchbox::mojom::TabAttachment::New();
  tab_attachment->tab_id = tab_id;
  tab_attachment->title = base::UTF16ToUTF8(TabUIHelper::From(tab)->GetTitle());
  tab_attachment->url = tab->GetContents()->GetLastCommittedURL();
  context->file_infos.push_back(
      searchbox::mojom::SearchContextAttachment::NewTabAttachment(
          std::move(tab_attachment)));

  searchbox_context_data->SetPendingContext(std::move(context));

  edit_model()->OpenAiMode(false, /*via_context_menu=*/false);
  std::move(callback).Run(base::ok(base::UnguessableToken::Create()));
}
void WebuiOmniboxHandler::StepSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) {
  searchbox::mojom::SelectionStep mojom_step;
  switch (step) {
    case OmniboxPopupSelection::Step::kWholeLine: {
      mojom_step = searchbox::mojom::SelectionStep::kWholeLine;
      break;
    }
    case OmniboxPopupSelection::Step::kStateOrLine: {
      mojom_step = searchbox::mojom::SelectionStep::kStateOrLine;
      break;
    }
    case OmniboxPopupSelection::Step::kAllLines: {
      mojom_step = searchbox::mojom::SelectionStep::kAllLines;
      break;
    }
    default: {
      NOTREACHED();
    }
  }
  page_->StepSelection(direction == OmniboxPopupSelection::kForward
                           ? searchbox::mojom::SelectionDirection::kForward
                           : searchbox::mojom::SelectionDirection::kBackward,
                       mojom_step);
}

void WebuiOmniboxHandler::OpenCurrentSelection(
    WindowOpenDisposition disposition) {
  page_->OpenCurrentSelection(disposition);
}

void WebuiOmniboxHandler::SetAimButtonVisible(bool visible) {
  page_->SetAimButtonVisible(visible);
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

  // Override contextual search spark loupe icon for GROUP_CONTEXTUAL_SEARCH.
  // Results on the omnibox webui will use an arrow icon instead.
  if (mojom_match &&
      match.suggestion_group_id == omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH) {
    mojom_match.value()->icon_path =
        searchbox_internal::kReplyRotated180IconResourceName;
  }

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

// TODO(crbug.com/469098088): Use something other than
//   `AutocompleteController::Observer::OnStart()` to reduce the IPC overhead
//   due to the fact that `AutocompleteController::Start()` gets invoked on
//   *every* keystroke in the Omnibox.
void WebuiOmniboxHandler::OnStart(AutocompleteController* controller,
                                  const AutocompleteInput& input) {
  const AutocompleteProviderClient* client =
      autocomplete_controller()->autocomplete_provider_client();
  // Check if there are zero suggest (either on NTP or on web) or the
  // input text is empty (necessary because `IsZeroSuggest()` is false on
  // clobber).
  page_->UpdateLensSearchEligibility(
      ContextualSearchProvider::LensEntrypointEligible(input, client) &&
      (input.IsZeroSuggest() || input.text().empty()));
}

void WebuiOmniboxHandler::OnResultChanged(AutocompleteController* controller,
                                          bool default_match_changed) {
  // TODO: (crbug.com/506266490) - Clean up these metrics since it is no longer
  // relevant to track after moving to factory pattern for searchbox::mojom.
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("FirstAccess")) {
    metrics_reporter_->Mark("FirstAccess");
    base::UmaHistogramBoolean(
        "Omnibox.Popup.WebUI.PageRemoteIsBoundOnFirstCall", true);
  }

  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("ResultChanged")) {
    metrics_reporter_->Mark("ResultChanged");
  }
  SearchboxHandler::OnResultChanged(controller, default_match_changed);
}

void WebuiOmniboxHandler::OnSelectionChanged(
    OmniboxPopupSelection old_selection,
    OmniboxPopupSelection selection) {
  page_->UpdateSelection(
      searchbox::mojom::OmniboxPopupSelection::New(
          old_selection.line, ConvertLineState(old_selection.state),
          old_selection.action_index),
      searchbox::mojom::OmniboxPopupSelection::New(
          selection.line, ConvertLineState(selection.state),
          selection.action_index));
}

void WebuiOmniboxHandler::OnKeywordStateChanged(bool is_keyword_selected) {
  page_->SetKeywordSelected(is_keyword_selected);
}

void WebuiOmniboxHandler::OnCharTyped(base::TimeTicks timestamp) {
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("CharTyped")) {
    metrics_reporter_->Mark("CharTyped", timestamp);
  }
}

void WebuiOmniboxHandler::OnActiveTabChanged(TabListInterface& tab_list,
                                             tabs::TabInterface* tab) {
  web_contents_observer_.ScopedObserve(tab->GetContents());
  ContextualSearchboxHandler::OnActiveTabChanged(tab_list, tab);
}

void WebuiOmniboxHandler::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  edit_model_observation_.Reset();
  autocomplete_controller_observation_.Reset();
  controller_ = nullptr;
  UpdateTabListObservation(nullptr);
}

void WebuiOmniboxHandler::OnTabDidInsert(tabs::TabInterface* tab) {
  if (auto* browser_window_interface = tab->GetBrowserWindowInterface()) {
    if (auto* location_bar =
            browser_window_interface->GetFeatures().location_bar()) {
      if (auto* omnibox_controller = location_bar->GetOmniboxController()) {
        edit_model_observation_.Reset();
        autocomplete_controller_observation_.Reset();
        controller_ = omnibox_controller;
        autocomplete_controller_observation_.Observe(autocomplete_controller());
        edit_model_observation_.Observe(omnibox_controller->edit_model());
        if (auto* helper = OmniboxPopupWebContentsHelper::FromWebContents(
                web_contents_.get())) {
          helper->set_omnibox_controller(omnibox_controller);
        }
        UpdateTabListObservation(
            TabListInterface::From(browser_window_interface));
      }
    }
  }
}

WebuiOmniboxHandler::WebContentsObserver::WebContentsObserver(
    WebuiOmniboxHandler* handler,
    content::WebContents* web_contents)
    : handler_(handler) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (browser_window_interface) {
    if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
      if (auto* active_tab = tab_list->GetActiveTab()) {
        Observe(active_tab->GetContents());
      }
    }
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

int WebuiOmniboxHandler::GetContextMenuMaxTabSuggestions() {
  omnibox::InputState input_state = GetInputState();
  if (auto it = input_state.max_inputs_by_type.find(
          omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
      it != input_state.max_inputs_by_type.end()) {
    return it->second;
  }
  return omnibox::kContextMenuMaxTabSuggestions.Get();
}

void WebuiOmniboxHandler::OnContentSharingPolicyChanged() {
  page_->UpdateContentSharingPolicy(
      contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs()));
}

void WebuiOmniboxHandler::OnAimPopupEligibilityChanged() {
  InitializeInputStateModel();

  page_->UpdateAimPopupEligibility(
      omnibox::IsAimPopupEnabled(profile_) &&
      profile_->GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton));
}

void WebuiOmniboxHandler::OnNavigationFinished(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame()) {
    page_->OnTabStripChanged();
  }
}
