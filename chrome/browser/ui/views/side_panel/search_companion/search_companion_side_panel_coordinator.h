// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_view.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"

class Browser;
class SidePanelRegistry;

namespace optimization_guide {
class NewOptimizationGuideDecider;
}  // namespace optimization_guide

namespace views {
class View;
}  // namespace views

// SearchCompanionSidePanelCoordinator handles the creation and registration of
// the search companion SidePanelEntry.
class SearchCompanionSidePanelCoordinator
    : public BrowserUserData<SearchCompanionSidePanelCoordinator>,
      public ZeroSuggestCacheService::Observer {
 public:
  explicit SearchCompanionSidePanelCoordinator(Browser* browser);
  SearchCompanionSidePanelCoordinator(
      const SearchCompanionSidePanelCoordinator&) = delete;
  SearchCompanionSidePanelCoordinator& operator=(
      const SearchCompanionSidePanelCoordinator&) = delete;
  ~SearchCompanionSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // ZeroSuggestCacheService::Observer:
  void OnZeroSuggestResponseUpdated(
      const std::string& page_url,
      const ZeroSuggestCacheService::CacheEntry& response) override;

  bool Show();
  BrowserView* GetBrowserView();

 private:
  base::WeakPtr<search_companion::SearchCompanionSidePanelView>
      side_panel_view_;

  void HandleOptGuidePageEntitiesResponse(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::NewOptimizationGuideDecider> opt_guide_ = nullptr;

  // Tracks the observed ZeroSuggestCacheService, for cleanup.
  base::ScopedObservation<ZeroSuggestCacheService,
                          ZeroSuggestCacheService::Observer>
      zero_suggest_cache_service_observation_{this};

  std::string latest_page_url_;
  std::string latest_suggest_response_string_;
  std::string latest_content_annotation_string_;

  friend class BrowserUserData<SearchCompanionSidePanelCoordinator>;

  std::unique_ptr<views::View> CreateCompanionWebView();
  void UpdateContentAnnotations(
      const optimization_guide::proto::PageEntitiesMetadata& entities_metadata);
  void UpdateSidePanelContent();

  base::WeakPtrFactory<SearchCompanionSidePanelCoordinator> weak_ptr_factory_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_
