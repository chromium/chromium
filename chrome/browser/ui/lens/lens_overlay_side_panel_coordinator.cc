// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"

#include <vector>

#include "base/feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_composebox_controller.h"
#include "chrome/browser/ui/lens/lens_help_menu_utils.h"
#include "chrome/browser/ui/lens/lens_media_link_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_web_view.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/lens/page_content_type_conversions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_side_panel_menu_option.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/lens/lens_url_utils.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace lens {

namespace {

inline constexpr char kChromeSideSearchVersionHeaderName[] =
    "X-Chrome-Side-Search-Version";
inline constexpr char kChromeSideSearchVersionHeaderValue[] = "1";
inline constexpr int kSidePanelPreferredDefaultWidth = 440;

// Checks to see if the navigation is a same document navigation that is not in
// the iframe. This is used to ignore navigations that are not relevant to the
// results in the side panel iframe.
bool IsIframesResultsNavigation(content::NavigationHandle* navigation_handle) {
  const GURL& nav_url = navigation_handle->GetURL();
  return navigation_handle->IsRendererInitiated() &&
         nav_url.SchemeIsHTTPOrHTTPS() && !navigation_handle->IsSameDocument() &&
         !navigation_handle->IsInPrimaryMainFrame() &&
         navigation_handle->GetParentFrame() &&
         navigation_handle->GetParentFrame()->IsInPrimaryMainFrame();
}

bool IsSiteTrusted(const GURL& url) {
  if (google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return true;
  }

  // This is a workaround for local development where the URL may be a
  // non-Google domain / proxy. If the Finch flag for the lens overlay results
  // search URL is not set to a Google domain, make sure the request is coming
  // from the results search URL page.
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, GURL(lens::features::GetLensOverlayResultsSearchURL()),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return true;
  }

  return false;
}

SidePanelUI* GetSidePanelUI(LensSearchController* controller) {
  return controller->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->GetFeatures()
      .side_panel_ui();
}

}  // namespace

SearchQuery::SearchQuery(std::string text_query, GURL url)
    : search_query_text_(std::move(text_query)),
      search_query_url_(std::move(url)) {}

SearchQuery::SearchQuery(const SearchQuery& other) {
  search_query_text_ = other.search_query_text_;
  if (other.selected_region_) {
    selected_region_ = other.selected_region_->Clone();
  }
  selected_region_bitmap_ = other.selected_region_bitmap_;
  selected_region_thumbnail_uri_ = other.selected_region_thumbnail_uri_;
  search_query_url_ = other.search_query_url_;
  selected_text_ = other.selected_text_;
  lens_selection_type_ = other.lens_selection_type_;
  translate_options_ = other.translate_options_;
}

SearchQuery& SearchQuery::operator=(const SearchQuery& other) {
  search_query_text_ = other.search_query_text_;
  if (other.selected_region_) {
    selected_region_ = other.selected_region_->Clone();
  }
  selected_region_bitmap_ = other.selected_region_bitmap_;
  selected_region_thumbnail_uri_ = other.selected_region_thumbnail_uri_;
  search_query_url_ = other.search_query_url_;
  selected_text_ = other.selected_text_;
  lens_selection_type_ = other.lens_selection_type_;
  translate_options_ = other.translate_options_;
  return *this;
}

SearchQuery::~SearchQuery() = default;

LensOverlaySidePanelCoordinator::LensOverlaySidePanelCoordinator(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}

LensOverlaySidePanelCoordinator::~LensOverlaySidePanelCoordinator() {
  // If the coordinator is destroyed before the web view, clear the reference
  // from the web view.
  if (side_panel_web_view_) {
    side_panel_web_view_->ClearCoordinator();
    side_panel_web_view_ = nullptr;
  }
}

void LensOverlaySidePanelCoordinator::RegisterEntryAndShow() {
  if (state_ != State::kOff) {
    // Exit early if the side panel is already registered or opening.
    return;
  }

  state_ = State::kOpeningSidePanel;
  RegisterEntry();
  GetSidePanelUI(GetLensSearchController())
      ->Show(SidePanelEntry::Id::kLensOverlayResults);
  GetLensOverlayController()->NotifyResultsPanelOpened();

  // Create the initialization data for this journey.
  initialization_data_ = std::make_unique<SidePanelInitializationData>();
}

SidePanelEntry::PanelType LensOverlaySidePanelCoordinator::GetPanelType()
    const {
  return SidePanelEntry::PanelType::kContent;
}

void LensOverlaySidePanelCoordinator::RecordAndShowSidePanelErrorPage() {
  CHECK(side_panel_page_);
  side_panel_page_->SetShowErrorPage(side_panel_should_show_error_page_,
                                     side_panel_result_status_);
  lens::RecordSidePanelResultStatus(
      static_cast<lens::SidePanelResultStatus>(side_panel_result_status_));
}

void LensOverlaySidePanelCoordinator::SetSidePanelNewTabUrl(const GURL& url) {
  side_panel_new_tab_url_ = lens::RemoveSidePanelURLParameters(url);
}

void LensOverlaySidePanelCoordinator::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  GetLensSearchController()->OnSidePanelWillHide(reason);
}

void LensOverlaySidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  // We cannot distinguish between:
  //   (1) A teardown during the middle of the async close process from
  //   LensOverlayController.
  //   (2) The user clicked the 'x' button while the overlay is showing.
  //   (3) The side panel naturally went away after a tab switch.
  // Forward to LensOverlayController to have it disambiguate.
  GetLensSearchController()->OnSidePanelHidden();
}

void LensOverlaySidePanelCoordinator::WebViewClosing() {
  // This is called from the destructor of the WebView. Synchronously clear all
  // state associated with the WebView.
  if (side_panel_web_view_) {
    GetLensSearchboxController()->ResetSidePanelSearchboxHandler();
    side_panel_web_view_ = nullptr;
  }
}

content::WebContents*
LensOverlaySidePanelCoordinator::GetSidePanelWebContents() {
  if (side_panel_web_view_) {
    return side_panel_web_view_->GetWebContents();
  }
  return nullptr;
}

bool LensOverlaySidePanelCoordinator::MaybeHandleTextDirectives(
    const GURL& nav_url) {
  if (ShouldHandleTextDirectives(nav_url)) {
    const GURL& page_url = lens_search_controller_->GetTabInterface()
                               ->GetContents()
                               ->GetLastCommittedURL();
    // Need to check if the page URL matches the navigation URL again. This is
    // because in the case of the navigation URL being a search URL with a text
    // fragment, it should open in a new tab instead of the side panel. This
    // also adds an additional check to make sure the text query parameters
    // match.
    if (lens::IsValidSearchResultsUrl(nav_url)) {
      auto page_url_text_query = lens::ExtractTextQueryParameterValue(page_url);
      auto nav_url_text_query = lens::ExtractTextQueryParameterValue(nav_url);
      if (page_url.GetHost() != nav_url.GetHost() ||
          page_url.GetPath() != nav_url.GetPath() ||
          page_url_text_query != nav_url_text_query) {
        lens::RecordHandleTextDirectiveResult(
            lens::LensOverlayTextDirectiveResult::kOpenedInNewTab);
        lens_search_controller_->GetTabInterface()
            ->GetBrowserWindowInterface()
            ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
        return true;
      }
    }

    // Nav url should have a text fragment.
    auto text_fragments =
        shared_highlighting::ExtractTextFragments(nav_url.GetRef());

    // Create and attach a `TextFinderManager` to the primary page.
    content::Page& page = lens_search_controller_->GetTabInterface()
                              ->GetContents()
                              ->GetPrimaryPage();
    companion::TextFinderManager* text_finder_manager =
        companion::TextFinderManager::GetOrCreateForPage(page);
    text_finder_manager->CreateTextFinders(
        text_fragments,
        base::BindOnce(
            &LensOverlaySidePanelCoordinator::OnTextFinderLookupComplete,
            weak_ptr_factory_.GetWeakPtr(), nav_url));
    return true;
  }
  return false;
}

bool LensOverlaySidePanelCoordinator::MaybeHandleContextualMediaLink(
    const GURL& nav_url) {
  // Exit early if the feature is disabled or the overlay is showing.
  if (!lens::features::IsLensVideoCitationsEnabled() ||
      GetLensOverlayController()->IsOverlayShowing()) {
    return false;
  }

  return lens::LensMediaLinkHandler(
             lens_search_controller_->GetTabInterface()->GetContents())
      .MaybeReplaceNavigation(nav_url);
}

bool LensOverlaySidePanelCoordinator::IsEntryShowing() {
  auto* side_panel_ui = GetSidePanelUI(GetLensSearchController());
  if (!side_panel_ui) {
    return false;
  }

  return side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
}

void LensOverlaySidePanelCoordinator::NotifyNewQueryLoaded(std::string query,
                                                           GURL search_url) {
  CHECK(initialization_data_);

  // If we are loading the query that was just popped, do not add it to the
  // stack.
  auto loaded_search_query =
      initialization_data_->currently_loaded_search_query_;
  if (loaded_search_query &&
      AreSearchUrlsEquivalent(loaded_search_query->search_query_url_,
                              search_url)) {
    return;
  }

  // A search URL without a Lens mode parameter indicates a click on a related
  // search or other in-SRP refinement. In this case, we should clear all
  // selection and thumbnail state.
  const std::string lens_mode = lens::ExtractLensModeParameterValue(search_url);
  if (lens_mode.empty()) {
    GetLensOverlayController()->SetAdditionalSearchQueryParams(
        /*additional_search_query_params=*/{});
    GetLensSearchboxController()->SetSearchboxThumbnail("");
    GetLensOverlayController()->ClearAllSelections();
    GetLensSearchboxController()->SetSearchboxThumbnail(std::string());
    GetLensSearchboxController()->SetShowSidePanelSearchboxThumbnail(false);
  } else {
    GetLensSearchboxController()->SetShowSidePanelSearchboxThumbnail(true);
  }

  // Grab the current state of the overlay and use it to update populate the
  // query stack and currently loaded query.
  lens::SearchQuery search_query(query, search_url);
  GetLensOverlayController()->AddOverlayStateToSearchQuery(search_query);
  GetLensSearchboxController()->AddSearchboxStateToSearchQuery(search_query);

  // Add what was the currently loaded search query to the query stack,
  // if it is present.
  if (loaded_search_query) {
    initialization_data_->search_query_history_stack_.push_back(
        loaded_search_query.value());
    SetBackArrowVisible(/*visible=*/true);
  }

  // Set the currently loaded search query to the one we just created.
  initialization_data_->currently_loaded_search_query_.reset();
  initialization_data_->currently_loaded_search_query_ = search_query;

  // Update searchbox and selection state to match the new query.
  GetLensSearchboxController()->SetSearchboxInputText(query);
}

void LensOverlaySidePanelCoordinator::PopAndLoadQueryFromHistory() {
  if (initialization_data_->search_query_history_stack_.empty()) {
    return;
  }
  base::Time query_start_time = base::Time::Now();

  // Get the query that should be loaded in the results frame and then pop it
  // from the list.
  auto query = initialization_data_->search_query_history_stack_.back();
  initialization_data_->search_query_history_stack_.pop_back();

  // If the query history stack is now empty, hide the back arrow.
  if (initialization_data_->search_query_history_stack_.empty()) {
    side_panel_page_->SetBackArrowVisible(false);
  }

  // Set the translate mode for the new query. Even if there are no translate
  // options, still need to pass the std::nullopt to disable translate
  // mode in the overlay.
  GetLensOverlayController()->SetTranslateMode(query.translate_options_);

  // Clear any active selections on the page and then re-add selections for this
  // query and update the selection, thumbnail and searchbox state.
  GetLensOverlayController()->ClearAllSelections();

  // Do not reset text selections for translated text since it may
  // not be on the screen until the full image request is resent.
  if (query.selected_text_.has_value() &&
      !query.translate_options_.has_value()) {
    GetLensOverlayController()->SetTextSelection(query.selected_text_->first,
                                                 query.selected_text_->second);
  } else if (query.selected_region_) {
    GetLensOverlayController()->SetPostRegionSelection(
        query.selected_region_->Clone());
  }
  GetLensOverlayController()->SetAdditionalSearchQueryParams(
      query.additional_search_query_params_);
  GetLensSearchboxController()->SetSearchboxInputText(query.search_query_text_);
  GetLensSearchboxController()->SetSearchboxThumbnail(
      query.selected_region_thumbnail_uri_);

  const bool is_contextual_query =
      GetLensOverlayController()->IsContextualSearchbox();
  const bool query_has_image =
      query.selected_region_ || !query.selected_region_bitmap_.drawsNothing();
  const bool should_send_interaction = query_has_image || is_contextual_query;

  if (should_send_interaction) {
    // If the current query has a region or image bytes, a new interaction
    // request needs to be sent to the server to keep the request IDs in sync
    // with the server. If not, the server will respond with broken SRP results.
    // Because of this, the currently loaded query should be modified so
    // duplicates don't get added to the query history stack.
    initialization_data_->currently_loaded_search_query_.reset();
    if (!initialization_data_->search_query_history_stack_.empty()) {
      auto previous_query =
          initialization_data_->search_query_history_stack_.back();
      initialization_data_->search_query_history_stack_.pop_back();
      initialization_data_->currently_loaded_search_query_ = previous_query;
    }
  }

  if (query_has_image) {
    std::optional<SkBitmap> selected_region_bitmap =
        query.selected_region_bitmap_.drawsNothing()
            ? std::nullopt
            : std::make_optional<SkBitmap>(query.selected_region_bitmap_);

    // If the query also has text, we should send it as a multimodal query.
    if (query.search_query_text_.empty()) {
      GetLensOverlayController()->IssueLensRequest(
          query_start_time, query.selected_region_->Clone(),
          query.lens_selection_type_, selected_region_bitmap);
    } else {
      // TODO(crbug.com/404941800): It might be better to send the multimodal
      // request directly to the query controller once the query controller is
      // owned by the search controller.
      GetLensOverlayController()->IssueMultimodalRequest(
          query_start_time, query.selected_region_->Clone(),
          query.search_query_text_, query.lens_selection_type_,
          selected_region_bitmap);
    }
    return;
  }

  // The query is text only. If we are in the contextual flow, resend the
  // contextual query.
  if (is_contextual_query) {
    // TODO(crbug.com/404941800): It might be better to send the contextual
    // request directly to the query controller once the query controller is
    // owned by the search controller.
    GetLensOverlayController()->IssueContextualTextRequest(
        query_start_time, query.search_query_text_, query.lens_selection_type_);
    return;
  }

  // Load the popped query URL in the results frame if it does not need to
  // send image bytes.
  LoadURLInResultsFrame(query.search_query_url_);

  // Set the currently loaded query to the one we just popped.
  initialization_data_->currently_loaded_search_query_.reset();
  initialization_data_->currently_loaded_search_query_ = query;
}

void LensOverlaySidePanelCoordinator::GetIsContextualSearchbox(
    GetIsContextualSearchboxCallback callback) {
  GetLensSearchboxController()->GetIsContextualSearchbox(std::move(callback));
}

void LensOverlaySidePanelCoordinator::RequestSendFeedback() {
  FeedbackRequestedByEvent(lens_search_controller_->GetTabInterface(),
                           ui::EF_NONE);
}

void LensOverlaySidePanelCoordinator::OnAimMessage(
    const std::vector<uint8_t>& message) {
  // Pass the message to the LensComposeboxController to handle.
  GetLensComposeboxController()->OnAimMessage(message);
}

void LensOverlaySidePanelCoordinator::OnImageQueryWithEmptyText() {
  // This flow is only triggered if at least one query was already issued.
  if (!initialization_data_->currently_loaded_search_query_.has_value()) {
    return;
  }

  // Copy the query but clear the text and URL.
  auto query = initialization_data_->currently_loaded_search_query_.value();
  query.search_query_text_ = std::string();
  query.search_query_url_ = GURL();

  // Update the selection type if it was previously a multimodal query.
  // Otherwise leave it be.
  if (query.lens_selection_type_ ==
          lens::LensOverlaySelectionType::MULTIMODAL_SEARCH ||
      query.lens_selection_type_ ==
          lens::LensOverlaySelectionType::MULTIMODAL_SUGGEST_TYPEAHEAD ||
      query.lens_selection_type_ ==
          lens::LensOverlaySelectionType::MULTIMODAL_SUGGEST_ZERO_PREFIX ||
      query.lens_selection_type_ ==
          lens::LensOverlaySelectionType::MULTIMODAL_SELECTION_CLEAR) {
    query.lens_selection_type_ =
        lens::LensOverlaySelectionType::MULTIMODAL_SELECTION_CLEAR;
  }

  base::Time query_start_time = base::Time::Now();

  // Clear any active selections on the page and then re-add selections for this
  // query and update the selection, thumbnail and searchbox state.
  GetLensOverlayController()->ClearAllSelections();

  const bool query_has_image =
      query.selected_region_ || !query.selected_region_bitmap_.drawsNothing();
  CHECK(query_has_image);

  std::optional<SkBitmap> selected_region_bitmap =
      query.selected_region_bitmap_.drawsNothing()
          ? std::nullopt
          : std::make_optional<SkBitmap>(query.selected_region_bitmap_);
  GetLensOverlayController()->IssueLensRequest(
      query_start_time, query.selected_region_->Clone(),
      query.lens_selection_type_, selected_region_bitmap);
}

void LensOverlaySidePanelCoordinator::OnScrollToMessage(
    const std::vector<std::string>& text_fragments,
    uint32_t pdf_page_number) {
  if (!latest_page_url_with_response_.SchemeIsFile() ||
      !latest_page_url_with_response_.is_valid()) {
    return;
  }

  const auto& latest_page_url_with_viewport_params =
      AddPDFScrollToParametersToUrl(latest_page_url_with_response_,
                                    text_fragments, pdf_page_number);
#if BUILDFLAG(ENABLE_PDF)
  content::WebContents* web_contents =
      lens_search_controller_->GetTabInterface()->GetContents();

  // If a PDFDocumentHelper is found attached to the current web contents,
  // that means that the PDF viewer is currently loaded in it.
  auto* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
  if (pdf_helper) {
    if (ShouldHandlePDFViewportChange(latest_page_url_with_viewport_params)) {
      pdf_extension_util::DispatchShouldUpdateViewportEvent(
          web_contents->GetPrimaryMainFrame(),
          latest_page_url_with_viewport_params);
      return;
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  // Open it in a new tab if the URL is no longer on the main tab.
  lens_search_controller_->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->OpenGURL(latest_page_url_with_viewport_params,
                 WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void LensOverlaySidePanelCoordinator::ExecuteCommand(int command_id,
                                                     int event_flags) {
  switch (command_id) {
    case COMMAND_MY_ACTIVITY: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kMyActivity);
      ActivityRequestedByEvent(lens_search_controller_->GetTabInterface(),
                               event_flags);
      break;
    }
    case COMMAND_LEARN_MORE: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kLearnMore);
      InfoRequestedByEvent(lens_search_controller_->GetTabInterface(),
                           event_flags);
      break;
    }
    case COMMAND_SEND_FEEDBACK: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kSendFeedback);
      FeedbackRequestedByEvent(lens_search_controller_->GetTabInterface(),
                               event_flags);
      break;
    }
    default: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kUnknown);
      NOTREACHED() << "Unknown option";
    }
  }
}

void LensOverlaySidePanelCoordinator::SetShowProtectedErrorPage(
    bool show_protected_error_page) {
  if (show_protected_error_page) {
    side_panel_should_show_error_page_ = true;
    side_panel_result_status_ =
        mojom::SidePanelResultStatus::kErrorPageShownProtected;
  } else if (side_panel_result_status_ ==
             mojom::SidePanelResultStatus::kErrorPageShownProtected) {
    side_panel_should_show_error_page_ = false;
    side_panel_result_status_ = mojom::SidePanelResultStatus::kResultShown;
  }

  if (side_panel_page_) {
    RecordAndShowSidePanelErrorPage();
  }
}

bool LensOverlaySidePanelCoordinator::IsShowingProtectedErrorPage() {
  return side_panel_should_show_error_page_ &&
         side_panel_result_status_ ==
             mojom::SidePanelResultStatus::kErrorPageShownProtected;
}

void LensOverlaySidePanelCoordinator::SetLatestPageUrlWithResponse(
    const GURL& url) {
  latest_page_url_with_response_ = url;
}

void LensOverlaySidePanelCoordinator::BindSidePanel(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) {
  // Ideally, this should be a CHECK, but if the user just navigates to the
  // WebUI link directly, then this will be called without
  // RegisterEntryAndShow() being called. If that is the case, ignore this call.
  // More info at crbug.com/417119042.
  if (state_ != State::kOpeningSidePanel) {
    return;
  }

  side_panel_receiver_.Bind(std::move(receiver));
  side_panel_page_.Bind(std::move(page));

  SetIsOverlayShowing(GetLensOverlayController()->IsOverlayShowing());
  if (pending_side_panel_url_.has_value()) {
    side_panel_page_->LoadResultsInFrame(*pending_side_panel_url_);
    pending_side_panel_url_.reset();
  }
  // Only record and show the side panel error state if the side panel was set
  // to do so. Otherwise, do nothing as this metric will be recorded when the
  // first side panel navigation completes.
  if (side_panel_should_show_error_page_) {
    RecordAndShowSidePanelErrorPage();
  }

  // Send the document type to the side panel when it is rendered for the first
  // time.
  NotifyPageContentUpdated();
}

bool LensOverlaySidePanelCoordinator::IsSidePanelBound() {
  return side_panel_page_.is_bound();
}

void LensOverlaySidePanelCoordinator::LoadURLInResultsFrame(const GURL& url) {
  CHECK(state_ != State::kOff) << "Side panel is not registered. You must "
                                  "first call RegisterEntryAndShow().";

  if (side_panel_page_) {
    side_panel_page_->LoadResultsInFrame(url);
    return;
  }

  // Store the URL and open the side panel. Once the side panel is opened and
  // communication is established, the URL will be loaded.
  pending_side_panel_url_ = std::make_optional<GURL>(url);
  RegisterEntryAndShow();
}

void LensOverlaySidePanelCoordinator::NotifyPageContentUpdated() {
  if (!side_panel_page_) {
    return;
  }

  auto page_content_type =
      StringMimeTypeToMojoPageContentType(GetLensOverlayController()
                                              ->GetTabInterface()
                                              ->GetContents()
                                              ->GetContentsMimeType());
  side_panel_page_->PageContentTypeChanged(page_content_type);
}

void LensOverlaySidePanelCoordinator::MaybeSetSidePanelShowErrorPage(
    bool should_show_error_page,
    mojom::SidePanelResultStatus status) {
  // Only show / hide the error page if the side panel is not already in that
  // state. Return early if the state should not change unless the initial load
  // has not been logged (`side_panel_result_status_` set to kUnknown).
  if (side_panel_should_show_error_page_ == should_show_error_page &&
      side_panel_result_status_ != mojom::SidePanelResultStatus::kUnknown) {
    return;
  }

  side_panel_should_show_error_page_ = should_show_error_page;
  side_panel_result_status_ = status;
  if (side_panel_page_) {
    RecordAndShowSidePanelErrorPage();
  }
}

void LensOverlaySidePanelCoordinator::SetSidePanelIsOffline(bool is_offline) {
  // If the side panel is already showing an error page, then this should be a
  // no-op.
  if (side_panel_result_status_ ==
          mojom::SidePanelResultStatus::kErrorPageShownStartQueryError ||
      side_panel_result_status_ ==
          mojom::SidePanelResultStatus::kErrorPageShownProtected) {
    return;
  }

  MaybeSetSidePanelShowErrorPage(
      is_offline, is_offline
                      ? mojom::SidePanelResultStatus::kErrorPageShownOffline
                      : mojom::SidePanelResultStatus::kResultShown);
}

void LensOverlaySidePanelCoordinator::SetSidePanelIsLoadingResults(
    bool is_loading) {
  if (side_panel_page_) {
    side_panel_page_->SetIsLoadingResults(is_loading);
  }
}

void LensOverlaySidePanelCoordinator::SetBackArrowVisible(bool visible) {
  if (side_panel_page_) {
    side_panel_page_->SetBackArrowVisible(visible);
  }
}

void LensOverlaySidePanelCoordinator::SetPageContentUploadProgress(
    double progress) {
  if (side_panel_page_) {
    side_panel_page_->SetPageContentUploadProgress(progress);
  }
}

void LensOverlaySidePanelCoordinator::SendClientMessageToAim(
    const std::vector<uint8_t>& serialized_message) {
  if (side_panel_page_) {
    side_panel_page_->SendClientMessageToAim(serialized_message);
  }
}

void LensOverlaySidePanelCoordinator::AimHandshakeReceived() {
  if (side_panel_page_) {
    side_panel_page_->AimHandshakeReceived();
  }
}

void LensOverlaySidePanelCoordinator::AimResultsChanged(bool on_aim) {
  // Close the overlay if the user transitions to the AIM UI.
  if (on_aim && lens::features::ShouldCloseOverlayOnAimTransition()) {
    lens_search_controller_->HideOverlay();
  }
  if (side_panel_page_) {
    side_panel_page_->AimResultsChanged(on_aim);
  }
}

void LensOverlaySidePanelCoordinator::SetIsOverlayShowing(bool is_showing) {
  if (base::FeatureList::IsEnabled(
          lens::features::kLensSearchReinvocationAffordance) &&
      side_panel_page_) {
    side_panel_page_->SetIsOverlayShowing(is_showing);
  }
}

void LensOverlaySidePanelCoordinator::FocusResultsFrame() {
  if (side_panel_page_) {
    side_panel_page_->FocusResultsFrame();
  }
}

void LensOverlaySidePanelCoordinator::SuppressGhostLoader() {
  if (side_panel_page_) {
    side_panel_page_->SuppressGhostLoader();
  }
}

void LensOverlaySidePanelCoordinator::FocusSearchbox() {
  auto* web_contents = GetSidePanelWebContents();
  if (web_contents && side_panel_page_) {
    web_contents->Focus();
    side_panel_page_->FocusSearchbox();
  }
}

LensOverlaySidePanelCoordinator::SidePanelInitializationData::
    SidePanelInitializationData() = default;
LensOverlaySidePanelCoordinator::SidePanelInitializationData::
    ~SidePanelInitializationData() = default;

void LensOverlaySidePanelCoordinator::DeregisterEntryAndCleanup() {
  auto* registry = lens_search_controller_->GetTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  CHECK(registry);

  // Remove the side panel entry observer if it is present.
  auto* registered_entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
  if (registered_entry) {
    registered_entry->RemoveObserver(this);
  }

  // This is a no-op if the entry does not exist.
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));

  // Cleanup internal state.
  side_panel_receiver_.reset();
  side_panel_page_.reset();
  initialization_data_.reset();
  pending_side_panel_url_.reset();
  side_panel_should_show_error_page_ = false;
  side_panel_new_tab_url_ = GURL();
  side_panel_result_status_ = mojom::SidePanelResultStatus::kUnknown;

  state_ = State::kOff;
}

// This method is called when the WebContents wants to open a link in a new
// tab (e.g. an anchor tag with target="_blank"). This delegate does not
// override AddNewContents(), so the WebContents is not actually created.
// Instead it forwards the parameters to the real browser. The navigation
// throttle is not sufficient to handle this because it only handles navigations
// within the same web contents.
void LensOverlaySidePanelCoordinator::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  // Ensure that the navigation is coming from a page we trust before
  // redirecting to main browser.
  if (!IsSiteTrusted(
          source_render_frame_host->GetLastCommittedOrigin().GetURL())) {
    return;
  }

  // This navigation is created from this component, so we consider it to be
  // browser initiated. In particular, we do not plumb all the parameters from
  // the original navigation. For instance we do not populate the
  // `initiator_frame_token`. This means some security properties like sandbox
  // flags are lost along the way.
  //
  // This is not problematic because we trust the original navigation was
  // initiated from the expected origin.
  //
  // Specifically, we need the navigation to be considered browser-initiated, as
  // renderer-initiated navigation history entries may be skipped if the
  // document does not receive any user interaction (like in our case). See
  // https://issuetracker.google.com/285038653
  content::OpenURLParams params(url, referrer, disposition, transition,
                                /*is_renderer_initiated=*/false);

  // We can't open a new tab while the observer is running because it might
  // destroy this WebContents. Post as task instead.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LensOverlaySidePanelCoordinator::OpenURLInBrowser,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void LensOverlaySidePanelCoordinator::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Focus the web contents immediately, so that hotkey presses (i.e. escape)
  // are handled.
  GetSidePanelWebContents()->Focus();
  SetSidePanelIsOffline(net::NetworkChangeNotifier::IsOffline());

  const GURL& nav_url = navigation_handle->GetURL();

  // We only care about the navigation if it is the results frame, is HTTPS,
  // renderer initiated and NOT a same document navigation.
  if (!IsIframesResultsNavigation(navigation_handle)) {
    return;
  }

  // Any navigation that the results iframe attempts to a different domain
  // will fail. Since the navigation throttle may not be able to intercept
  // certain navigations before they result in an error page, we should make
  // sure these error pages don't commit and instead open these URLs in a new
  // tab.
  if (!lens::IsValidSearchResultsUrl(nav_url) &&
      lens::GetSearchResultsUrlFromRedirectUrl(nav_url).is_empty()) {
    navigation_handle->SetSilentlyIgnoreErrors();

#if BUILDFLAG(ENABLE_PDF)
    content::WebContents* web_contents =
        lens_search_controller_->GetTabInterface()->GetContents();

    // If a PDFDocumentHelper is found attached to the current web contents,
    // that means that the PDF viewer is currently loaded in it.
    if (ShouldHandlePDFViewportChange(nav_url)) {
      auto* pdf_helper =
          pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
      if (pdf_helper) {
        pdf_extension_util::DispatchShouldUpdateViewportEvent(
            web_contents->GetPrimaryMainFrame(), nav_url);
        return;
      }
    }
#endif  // BUILDFLAG(ENABLE_PDF)

    // If the contextual search box is enabled, cross-origin navigations could
    // be a citation that should be rendered as text highlights in the current
    // tab.
    if (MaybeHandleTextDirectives(nav_url)) {
      return;
    }

    // If the contextual media link is enabled, cross-origin navigations could
    // be a video that should be played in the current tab.
    if (MaybeHandleContextualMediaLink(nav_url)) {
      return;
    }

    lens_search_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  // If the search URL should be opened in a new tab, open it here.
  auto* const profile = lens_search_controller_->GetTabInterface()
                            ->GetBrowserWindowInterface()
                            ->GetProfile();
  if (ShouldOpenSearchURLInNewTab(nav_url, lens::IsAimM3Enabled(profile))) {
    lens_search_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  // If the query has text directives, return early to allow the navigation
  // throttle to handle the request. Have to check `ShouldHandleTextDirectives`
  // separately in case the navigation happens to be a citation on a valid
  // search result that would typically be loaded in the side panel. In this
  // case, the navigation throttle will cancel the navigation and call
  // `MaybeHandleTextDirectives` directly.
  if (ShouldHandleTextDirectives(nav_url)) {
    return;
  }

  // If we expect to load this URL in the side panel, show the loading
  // page and any feature-specific request headers.
  navigation_handle->SetRequestHeader(kChromeSideSearchVersionHeaderName,
                                      kChromeSideSearchVersionHeaderValue);
  SetSidePanelNewTabUrl(GURL());

  // Notify the side panel that the results have moved to/from the AIM UI.
  const bool is_aim_query = IsAimQuery(nav_url);
  AimResultsChanged(is_aim_query);

  // If this is an AIM query, to be opened in the side panel, exit early to
  // prevent the ghost loader from being shown. AIM supports soft navigations to
  // handle custom animations, and showing the ghost loader would cover those.
  if (lens::features::GetSidePanelGhostLoaderDisabledForAim() && is_aim_query) {
    return;
  }
  SetSidePanelIsLoadingResults(true);
  // Notify the Composebox Controller that a new navigation has started so the
  // AIM handshake is no longer established.
  GetLensComposeboxController()->ResetAimHandshake();
}

void LensOverlaySidePanelCoordinator::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // We only care about loads that happen one level down in the side panel.
  if (!render_frame_host->GetParent() ||
      render_frame_host->GetParent()->GetParent() ||
      !render_frame_host->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  SetSidePanelNewTabUrl(render_frame_host->GetLastCommittedURL());
  SetSidePanelIsLoadingResults(false);
}

void LensOverlaySidePanelCoordinator::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore navigations that are not the final results frame navigation
  // initiated by the user.
  if (!IsIframesResultsNavigation(navigation_handle)) {
    return;
  }

  // Ignore navigations that were aborted due to user input. I.e the user
  // issued a new query.
  if (navigation_handle->GetNetErrorCode() == net::ERR_ABORTED) {
    return;
  }

  lens::RecordIframeLoadStatus(navigation_handle->IsErrorPage(),
                               navigation_handle->GetNetErrorCode());
}

web_modal::WebContentsModalDialogHost*
LensOverlaySidePanelCoordinator::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return lens_search_controller_->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->GetWebContentsModalDialogHostForWindow();
}

bool LensOverlaySidePanelCoordinator::ShouldHandleTextDirectives(
    const GURL& nav_url) {
  // Only handle text directives if the feature is enabled and the overlay is
  // not covering the current tab.
  if (!lens::features::HandleSidePanelTextDirectivesEnabled() ||
      GetLensOverlayController()->IsOverlayShowing()) {
    return false;
  }

  const GURL& page_url = lens_search_controller_->GetTabInterface()
                             ->GetContents()
                             ->GetLastCommittedURL();
  // Only handle text directives when the page URL and the URL being navigated
  // to have the same host and path, or if the URL being navigated to is result
  // search URL with a text fragment then it needs custom handling to open in a
  // new tab rather than in the side panel. This ignores the ref and query
  // attributes.
  if ((page_url.GetHost() != nav_url.GetHost() ||
       page_url.GetPath() != nav_url.GetPath()) &&
      !lens::IsValidSearchResultsUrl(nav_url)) {
    return false;
  }

  auto text_fragments =
      shared_highlighting::ExtractTextFragments(nav_url.GetRef());
  // If the url that is being navigated to does not have a text directive, then
  // it cannot be handled.
  return !text_fragments.empty();
}

bool LensOverlaySidePanelCoordinator::ShouldHandlePDFViewportChange(
    const GURL& nav_url) {
  // Only handle text directives if the feature is enabled and the overlay is
  // not covering the current tab.
  if (!lens::features::HandleSidePanelTextDirectivesEnabled() ||
      GetLensOverlayController()->IsOverlayShowing()) {
    return false;
  }

  const GURL& page_url = lens_search_controller_->GetTabInterface()
                             ->GetContents()
                             ->GetLastCommittedURL();
  // Handle the PDF hash change if the URL being navigated to is the same as the
  // URL loaded in the main tab. The URL being navigated to should also contain
  // a fragment with viewport parameters that will be parsed in the extension.
  return !nav_url.GetRef().empty() && page_url.GetHost() == nav_url.GetHost() &&
         page_url.GetPath() == nav_url.GetPath() &&
         page_url.GetQuery() == nav_url.GetQuery();
}

void LensOverlaySidePanelCoordinator::OnTextFinderLookupComplete(
    const GURL& nav_url,
    const std::vector<std::pair<std::string, bool>>& lookup_results) {
  const GURL& page_url = lens_search_controller_->GetTabInterface()
                             ->GetContents()
                             ->GetLastCommittedURL();
  if (lookup_results.empty()) {
    if (lens::features::IsLensSearchNotFoundOnPageToastEnabled() &&
        URLsMatchWithoutTextFragment(page_url, nav_url)) {
      lens::RecordHandleTextDirectiveResult(
          lens::LensOverlayTextDirectiveResult::kNotFoundOnPage);
      ShowToast(l10n_util::GetStringUTF8(
          IDS_LENS_OVERLAY_TOAST_PAGE_CONTENT_NOT_FOUND_MESSAGE));
      return;
    }

    lens::RecordHandleTextDirectiveResult(
        lens::LensOverlayTextDirectiveResult::kOpenedInNewTab);
    lens_search_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  std::vector<std::string> text_directives;
  for (auto pair : lookup_results) {
    // If any of the text fragments are not found, then open in a new tab.
    if (!pair.second) {
      if (lens::features::IsLensSearchNotFoundOnPageToastEnabled() &&
          URLsMatchWithoutTextFragment(page_url, nav_url)) {
        lens::RecordHandleTextDirectiveResult(
            lens::LensOverlayTextDirectiveResult::kNotFoundOnPage);
        ShowToast(l10n_util::GetStringUTF8(
            IDS_LENS_OVERLAY_TOAST_PAGE_CONTENT_NOT_FOUND_MESSAGE));
        return;
      }

      lens::RecordHandleTextDirectiveResult(
          lens::LensOverlayTextDirectiveResult::kOpenedInNewTab);
      lens_search_controller_->GetTabInterface()
          ->GetBrowserWindowInterface()
          ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
      return;
    }
    text_directives.push_back(pair.first);
  }

  // Delete any existing `TextHighlighterManager` on the page. Without this, any
  // text highlights after the first to be rendered on the page will not render.
  auto& page = lens_search_controller_->GetTabInterface()
                   ->GetContents()
                   ->GetPrimaryPage();
  if (companion::TextHighlighterManager::GetForPage(page)) {
    companion::TextHighlighterManager::DeleteForPage(page);
  }

  // If every text fragment was found, then create a text highlighter manager to
  // render the text highlights. Focus the main tab first.
  lens::RecordHandleTextDirectiveResult(
      lens::LensOverlayTextDirectiveResult::kFoundOnPage);
  lens_search_controller_->GetTabInterface()->GetContents()->Focus();
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetOrCreateForPage(page);
  text_highlighter_manager->CreateTextHighlightersAndRemoveExisting(
      text_directives);
}

void LensOverlaySidePanelCoordinator::OpenURLInBrowser(
    const content::OpenURLParams& params) {
  lens_search_controller_->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->OpenURL(params, /*navigation_handle_callback=*/{});
}

void LensOverlaySidePanelCoordinator::RegisterEntry() {
  auto* registry = lens_search_controller_->GetTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  CHECK(registry);

  // If the entry is already registered, don't register it again.
  if (!registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults))) {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView,
            base::Unretained(this)),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl,
            base::Unretained(this)),
        GetMoreInfoCallback(),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::GetPreferredDefaultWidth,
            base::Unretained(this)));
    entry->SetProperty(kShouldShowTitleInSidePanelHeaderKey, false);
    registry->Register(std::move(entry));

    // Observe the side panel entry.
    auto* registered_entry = registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
    registered_entry->AddObserver(this);
  }
}

std::unique_ptr<views::View>
LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView(
    SidePanelEntryScope& scope) {
  // TODO(crbug.com/328295358): Change task manager string ID in view creation
  // when available.
  auto view = std::make_unique<LensOverlaySidePanelWebView>(
      lens_search_controller_->GetTabInterface()
          ->GetContents()
          ->GetBrowserContext(),
      this, scope);
  view->SetProperty(views::kElementIdentifierKey,
                    LensOverlayController::kOverlaySidePanelWebViewId);
  side_panel_web_view_ = view.get();
  Observe(GetSidePanelWebContents());

  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

GURL LensOverlaySidePanelCoordinator::GetSidePanelNewTabUrl() {
  return lens::GetSidePanelNewTabUrl(
      side_panel_new_tab_url_,
      GetLensOverlayQueryController()->GetVsridForNewTab());
}

void LensOverlaySidePanelCoordinator::ShowToast(std::string message) {
  if (side_panel_page_) {
    side_panel_page_->ShowToast(message);
  }
}

GURL LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl() {
  if (lens::features::IsLensOverlaySidePanelOpenInNewTabEnabled()) {
    return GetSidePanelNewTabUrl();
  } else {
    return GURL();
  }
}

int LensOverlaySidePanelCoordinator::GetPreferredDefaultWidth() {
  return kSidePanelPreferredDefaultWidth;
}

base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
LensOverlaySidePanelCoordinator::GetMoreInfoCallback() {
  if (lens::IsLensOverlayContextualSearchboxEnabled()) {
    return base::BindRepeating(
        &LensOverlaySidePanelCoordinator::GetMoreInfoMenuModel,
        base::Unretained(this));
  }
  return base::NullCallbackAs<std::unique_ptr<ui::MenuModel>()>();
}

std::unique_ptr<ui::MenuModel>
LensOverlaySidePanelCoordinator::GetMoreInfoMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  menu_model->AddItemWithIcon(
      COMMAND_MY_ACTIVITY,
      l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_MY_ACTIVITY),
      ui::ImageModel::FromVectorIcon(vector_icons::kGoogleGLogoMonochromeIcon,
                                     ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
#else
  menu_model->AddItem(COMMAND_MY_ACTIVITY,
                      l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_MY_ACTIVITY));
#endif
  menu_model->AddItemWithIcon(
      COMMAND_LEARN_MORE,
      l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_LEARN_MORE),
      ui::ImageModel::FromVectorIcon(vector_icons::kInfoOutlineIcon,
                                     ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));

  if (!lens::features::IsLensSearchSidePanelNewFeedbackEnabled()) {
    menu_model->AddItemWithIcon(
        COMMAND_SEND_FEEDBACK,
        l10n_util::GetStringUTF16(IDS_LENS_SEND_FEEDBACK),
        ui::ImageModel::FromVectorIcon(vector_icons::kFeedbackIcon,
                                       ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
  }
  return menu_model;
}

}  // namespace lens
