// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_query_flow_router.h"

#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_mime_type.h"

namespace lens {

LensQueryFlowRouter::LensQueryFlowRouter(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}

LensQueryFlowRouter::~LensQueryFlowRouter() = default;

bool LensQueryFlowRouter::IsOff() const {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    auto* contextual_search_web_contents_helper =
        ContextualSearchWebContentsHelper::FromWebContents(web_contents());
    const bool has_contextual_search_session_handle =
        contextual_search_web_contents_helper &&
        contextual_search_web_contents_helper->session_handle();
    return !has_contextual_search_session_handle;
  }
  return lens_overlay_query_controller()->IsOff();
}

void LensQueryFlowRouter::StartQueryFlow(
    const SkBitmap& screenshot,
    GURL page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    base::span<const PageContent> underlying_page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> pdf_current_page,
    float ui_scale_factor,
    base::TimeTicks invocation_time) {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    // Create a contextual session for this WebContents if one does not exist.
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile());
    // TODO(crbug.com/463400248): Use contextual tasks config params for Lens
    // requests.
    CHECK(!pending_session_handle_);
    pending_session_handle_ = contextual_search_service->CreateSession(
        ntp_composebox::CreateQueryControllerConfigParams(),
        contextual_search::ContextualSearchSource::kLens);

    // TODO(crbug.com/462487081): Add support for sending the text response from
    // the full image request back to the overlay.
    return;
  }

  lens_overlay_query_controller()->StartQueryFlow(
      screenshot, page_url, page_title, std::move(significant_region_boxes),
      underlying_page_contents, primary_content_type, pdf_current_page,
      ui_scale_factor, invocation_time);
}

void LensQueryFlowRouter::MaybeRestartQueryFlow() {
  lens_overlay_query_controller()->MaybeRestartQueryFlow();
}

void LensQueryFlowRouter::SendRegionSearch(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  // TODO(crbug.com/456472761): When interaction requests are supported, send
  // these to the contextual search service. For now, open the contextual tasks
  // panel in the zero state.
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    OpenContextualTasksPanel();
    return;
  }

  lens_overlay_query_controller()->SendRegionSearch(
      query_start_time, std::move(region), lens_selection_type,
      additional_search_query_params, region_bytes);
}

void LensQueryFlowRouter::SendTextOnlyQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  // TODO(crbug.com/456472761): When interaction requests are supported, send
  // these to the contextual search service. For now, open the contextual tasks
  // panel in the zero state.
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    OpenContextualTasksPanel();
    return;
  }

  lens_overlay_query_controller()->SendTextOnlyQuery(
      query_start_time, query_text, lens_selection_type,
      additional_search_query_params);
}

void LensQueryFlowRouter::SendContextualTextQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  // TODO(crbug.com/456472761): When interaction requests are supported, send
  // these to the contextual search service. For now, open the contextual tasks
  // panel in the zero state.
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    OpenContextualTasksPanel();
    return;
  }

  lens_overlay_query_controller()->SendContextualTextQuery(
      query_start_time, query_text, lens_selection_type,
      additional_search_query_params);
}

void LensQueryFlowRouter::SendMultimodalRequest(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  // TODO(crbug.com/456472761): When interaction requests are supported, send
  // these to the contextual search service. For now, open the contextual tasks
  // panel in the zero state.
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    OpenContextualTasksPanel();
    return;
  }

  lens_overlay_query_controller()->SendMultimodalRequest(
      query_start_time, std::move(region), query_text, lens_selection_type,
      additional_search_query_params, region_bytes);
}

void LensQueryFlowRouter::OpenContextualTasksPanel() {
  // TODO(crbug.com/461909986): This should instead do an appropriate
  // interaction request which should then open the response URL in the panel.
  // Show the side panel. This will create a new task and associate it with the
  // active tab.
  contextual_tasks::ContextualTasksSidePanelCoordinator::From(
      browser_window_interface())
      ->Show();
}

}  // namespace lens
