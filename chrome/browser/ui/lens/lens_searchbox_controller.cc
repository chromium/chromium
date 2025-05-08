// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_searchbox_controller.h"

#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/sessions/core/session_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace lens {

LensSearchboxController::LensSearchboxController(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}
LensSearchboxController::~LensSearchboxController() = default;

const GURL& LensSearchboxController::GetPageURL() const {
  // TODO(crbug.com/413138792): Implement this method.
  return GURL::EmptyGURL();
}

SessionID LensSearchboxController::GetTabId() const {
  // TODO(crbug.com/413138792): Implement this method.
  return SessionID::InvalidValue();
}

metrics::OmniboxEventProto::PageClassification
LensSearchboxController::GetPageClassification() const {
  // TODO(crbug.com/413138792): Implement this method.
  return metrics::OmniboxEventProto::INVALID_SPEC;
}

std::string& LensSearchboxController::GetThumbnail() {
  // TODO(crbug.com/413138792): Implement this method.
  return selected_region_thumbnail_uri_;
}

const lens::proto::LensOverlaySuggestInputs&
LensSearchboxController::GetLensSuggestInputs() const {
  // TODO(crbug.com/413138792): Implement this method.
  return lens::proto::LensOverlaySuggestInputs().default_instance();
}

void LensSearchboxController::OnTextModified() {
  // TODO(crbug.com/413138792): Implement this method.
}

void LensSearchboxController::OnThumbnailRemoved() {
  // TODO(crbug.com/413138792): Implement this method.
}

void LensSearchboxController::OnSuggestionAccepted(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  // TODO(crbug.com/413138792): Implement this method.
}

void LensSearchboxController::OnFocusChanged(bool focused) {
  // TODO(crbug.com/413138792): Implement this method.
}

void LensSearchboxController::OnPageBound() {
  // TODO(crbug.com/413138792): Implement this method.
}

void LensSearchboxController::ShowGhostLoaderErrorState() {
  // TODO(crbug.com/413138792): Implement this method.
}

void LensSearchboxController::OnZeroSuggestShown() {
  // TODO(crbug.com/413138792): Implement this method.
}

}  // namespace lens
