// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion.mojom.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#include "services/data_decoder/public/cpp/data_decoder.h"

class SearchCompanionSidePanelUI;

namespace optimization_guide {
class NewOptimizationGuideDecider;
}  // namespace optimization_guide

///////////////////////////////////////////////////////////////////////////////
// SearchCompanionPageHandler
//
//  A handler of the Search Companion side panel WebUI (based on Polymer).
//  (chrome/browser/resources/side_panel/search_companion/app.ts).
//  This class is created and owned by SearchCompanionUI and has the same
//  lifetime as the Side Panel view.
//
class SearchCompanionPageHandler
    : public side_panel::mojom::SearchCompanionPageHandler,
      public ZeroSuggestCacheService::Observer {
 public:
  explicit SearchCompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandler>
          receiver,
      mojo::PendingRemote<side_panel::mojom::SearchCompanionPage> page,
      SearchCompanionSidePanelUI* search_companion_ui);
  SearchCompanionPageHandler(const SearchCompanionPageHandler&) = delete;
  SearchCompanionPageHandler& operator=(const SearchCompanionPageHandler&) =
      delete;
  ~SearchCompanionPageHandler() override;

  // side_panel::mojom::SearchCompanionPageHandler:
  void ShowUI() override;

  void NotifyUrlChanged(std::string new_url);
  void NotifyNewZeroSuggestPrefixData(std::string suggest_response);
  void NotifyNewOptimizationGuidePageAnnotations(
      std::string content_annotations);
  void NotifyNewViewportImages(std::string images_string);

  // ZeroSuggestCacheService::Observer:
  void OnZeroSuggestResponseUpdated(
      const std::string& page_url,
      const ZeroSuggestCacheService::CacheEntry& response) override;

 private:
  // Handle the output of page entity data once the appropriate server
  // call is made on page load.
  void HandleOptGuidePageEntitiesResponse(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Tracks the observed ZeroSuggestCacheService, for cleanup.
  base::ScopedObservation<ZeroSuggestCacheService,
                          ZeroSuggestCacheService::Observer>
      zero_suggest_cache_service_observation_{this};

  // Execute a script on the current primary frame's web content to pull the
  // image url of images on screen which fill certain filtering criteria.
  void ExecuteFetchImagesJavascript();
  // Handle the output of the fetch images javascript to ensure it is valid.
  void OnFetchImagesJavascriptResult(base::Value result);
  // Handle the output of the fetch images javascript after validity is ensured.
  void OnImageFetchJsonSanitizationCompleted(
      data_decoder::DataDecoder::ValueOrError result);

  base::RepeatingTimer fetch_images_timer_;

  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::NewOptimizationGuideDecider> opt_guide_ = nullptr;

  mojo::Receiver<side_panel::mojom::SearchCompanionPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::SearchCompanionPage> page_;
  raw_ptr<SearchCompanionSidePanelUI> search_companion_ui_ = nullptr;
  raw_ptr<Browser> browser_;

  base::WeakPtrFactory<SearchCompanionPageHandler> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_PAGE_HANDLER_H_
