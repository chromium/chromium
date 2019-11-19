// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_WEBUI_NTP_TILES_INTERNALS_MESSAGE_HANDLER_H_
#define COMPONENTS_NTP_TILES_WEBUI_NTP_TILES_INTERNALS_MESSAGE_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/ntp_tiles/most_visited_sites.h"

namespace base {
class ListValue;
}  // namespace base

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace ntp_tiles {

class MostVisitedSites;
class NTPTilesInternalsMessageHandlerClient;

// Implements the WebUI message handler for chrome://ntp-tiles-internals/
//
// Because content and iOS use different implementations of WebUI, this class
// implements the generic portion and depends on the embedder to inject a bridge
// to the embedder's API. It cannot itself implement either API directly.
class NTPTilesInternalsMessageHandler : public MostVisitedSites::Observer {
 public:
  // |favicon_service| must not be null and must outlive this object.
  explicit NTPTilesInternalsMessageHandler(
      favicon::FaviconService* favicon_service);
  ~NTPTilesInternalsMessageHandler() override;

  // Called when the WebUI page's JavaScript has loaded and it is ready to
  // receive RegisterMessageCallback() calls. |client| must outlive this object.
  void RegisterMessages(NTPTilesInternalsMessageHandlerClient* client);

 private:
  using FaviconResultMap = std::map<std::pair<GURL, favicon_base::IconType>,
                                    favicon_base::FaviconRawBitmapResult>;

  // Callbacks registered in RegisterMessages().
  void HandleRegisterForEvents(const base::ListValue* args);
  void HandleUpdate(const base::ListValue* args);
  void HandleFetchSuggestions(const base::ListValue* args);
  void HandleViewPopularSitesJson(const base::ListValue* args);

  void SendSourceInfo();
  void SendTiles(const NTPTilesVector& tiles,
                 const FaviconResultMap& result_map);

  // MostVisitedSites::Observer.
  void OnURLsAvailable(
      const std::map<SectionType, NTPTilesVector>& sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  void OnFaviconLookupDone(const NTPTilesVector& tiles,
                           FaviconResultMap* result_map,
                           size_t* num_pending_lookups,
                           const GURL& page_url,
                           const favicon_base::FaviconRawBitmapResult& result);

  favicon::FaviconService* favicon_service_;

  // Bridge to embedder's API.
  NTPTilesInternalsMessageHandlerClient* client_;

  int site_count_;
  std::unique_ptr<MostVisitedSites> most_visited_sites_;

  std::string suggestions_status_;
  std::string popular_sites_json_;

  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<NTPTilesInternalsMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NTPTilesInternalsMessageHandler);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_WEBUI_NTP_TILES_INTERNALS_MESSAGE_HANDLER_H_
