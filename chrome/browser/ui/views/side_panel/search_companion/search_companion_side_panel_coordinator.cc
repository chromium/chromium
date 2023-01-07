// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

using optimization_guide::proto::PageEntitiesMetadata;

SearchCompanionSidePanelCoordinator::SearchCompanionSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<SearchCompanionSidePanelCoordinator>(*browser),
      weak_ptr_factory_(this) {
  zero_suggest_cache_service_observation_.Observe(
      ZeroSuggestCacheServiceFactory::GetForProfile(browser->profile()));

  if (optimization_guide::features::RemotePageMetadataEnabled()) {
    opt_guide_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(browser->profile());

    if (opt_guide_) {
      std::vector<optimization_guide::proto::OptimizationType> types;
      types.push_back(
          optimization_guide::proto::OptimizationType::PAGE_ENTITIES);

      opt_guide_->RegisterOptimizationTypes(types);
    }
  }
}

SearchCompanionSidePanelCoordinator::~SearchCompanionSidePanelCoordinator() =
    default;

BrowserView* SearchCompanionSidePanelCoordinator::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

void SearchCompanionSidePanelCoordinator::UpdateContentAnnotations(
    const PageEntitiesMetadata& entities_metadata) {
  latest_content_annotation_string_ = "";
  for (const auto& category : entities_metadata.categories()) {
    if (category.category_id().empty()) {
      continue;
    }
    if (category.score() < 0 || category.score() > 100) {
      continue;
    }
    latest_content_annotation_string_ +=
        "Page Category: " + category.category_id() + "\n";
  }
  for (auto& entity : entities_metadata.entities()) {
    if (entity.entity_id().empty()) {
      continue;
    }
    if (entity.score() < 0 || entity.score() > 100) {
      continue;
    }
    latest_content_annotation_string_ +=
        "Page Entity: " + entity.entity_id() + "\n";
  }
}

void SearchCompanionSidePanelCoordinator::UpdateSidePanelContent() {
  if (side_panel_view_) {
    side_panel_view_->UpdateContent(latest_page_url_,
                                    latest_suggest_response_string_,
                                    latest_content_annotation_string_);
  }
}

void SearchCompanionSidePanelCoordinator::OnZeroSuggestResponseUpdated(
    const std::string& page_url,
    const ZeroSuggestCacheService::CacheEntry& response) {
  latest_page_url_ = page_url;
  latest_suggest_response_string_ = response.response_json;

  UpdateSidePanelContent();

  // Use zero suggest returning as the trigger to request entities from
  // optimization guide. In the future this probably should be triggered
  // by web navigation in the main frame.
  if (opt_guide_) {
    opt_guide_->CanApplyOptimization(
        GURL(page_url),
        optimization_guide::proto::OptimizationType::PAGE_ENTITIES,
        base::BindOnce(&SearchCompanionSidePanelCoordinator::
                           HandleOptGuidePageEntitiesResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SearchCompanionSidePanelCoordinator::HandleOptGuidePageEntitiesResponse(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }
  absl::optional<PageEntitiesMetadata> page_entities_metadata =
      metadata.ParsedMetadata<PageEntitiesMetadata>();
  if (page_entities_metadata) {
    PageEntitiesMetadata entities_metadata = *page_entities_metadata;
    UpdateContentAnnotations(entities_metadata);
    UpdateSidePanelContent();
  }
}

void SearchCompanionSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  std::u16string label(u"Search Companion");
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kSearchCompanion, label,
      ui::ImageModel::FromVectorIcon(kJourneysIcon, ui::kColorIcon,
                                     /*icon_size=*/16),
      base::BindRepeating(
          &SearchCompanionSidePanelCoordinator::CreateCompanionWebView,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
SearchCompanionSidePanelCoordinator::CreateCompanionWebView() {
  auto side_panel_view =
      std::make_unique<search_companion::SearchCompanionSidePanelView>(
          GetBrowserView());
  side_panel_view->UpdateContent("", "", "");
  side_panel_view_ = side_panel_view->GetWeakPtr();
  return std::move(side_panel_view);
}

bool SearchCompanionSidePanelCoordinator::Show() {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(&GetBrowser());
  if (!browser_view) {
    return false;
  }

  if (auto* side_panel_coordinator = browser_view->side_panel_coordinator()) {
    side_panel_coordinator->Show(SidePanelEntry::Id::kSearchCompanion);
  }

  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchCompanionSidePanelCoordinator);
