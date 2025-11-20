// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_query_flow_router.h"

#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "components/lens/lens_overlay_mime_type.h"

namespace lens {

LensQueryFlowRouter::LensQueryFlowRouter(LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}

LensQueryFlowRouter::~LensQueryFlowRouter() = default;

bool LensQueryFlowRouter::IsOff() const {
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
  lens_overlay_query_controller()->SendRegionSearch(
      query_start_time, std::move(region), lens_selection_type,
      additional_search_query_params, region_bytes);
}

void LensQueryFlowRouter::SendTextOnlyQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  lens_overlay_query_controller()->SendTextOnlyQuery(
      query_start_time, query_text, lens_selection_type,
      additional_search_query_params);
}

void LensQueryFlowRouter::SendContextualTextQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
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
  lens_overlay_query_controller()->SendMultimodalRequest(
      query_start_time, std::move(region), query_text, lens_selection_type,
      additional_search_query_params, region_bytes);
}

}  // namespace lens
