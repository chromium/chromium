// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEEDV2_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEEDV2_INTERNALS_PAGE_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefService;
namespace feed {
class FeedService;
class FeedStreamApi;
}  // namespace feed

// Concrete implementation of feed_internals::mojom::PageHandler.
class FeedV2InternalsPageHandler : public feed_internals::mojom::PageHandler {
 public:
  FeedV2InternalsPageHandler(
      mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver,
      feed::FeedService* feed_service,
      PrefService* pref_service);
  FeedV2InternalsPageHandler(const FeedV2InternalsPageHandler&) = delete;
  FeedV2InternalsPageHandler& operator=(const FeedV2InternalsPageHandler&) =
      delete;

  ~FeedV2InternalsPageHandler() override;

  // feed_internals::mojom::PageHandler
  void GetGeneralProperties(GetGeneralPropertiesCallback) override;
  void GetUserClassifierProperties(
      GetUserClassifierPropertiesCallback) override;
  void ClearUserClassifierProperties() override;
  void GetLastFetchProperties(GetLastFetchPropertiesCallback) override;
  void ClearCachedDataAndRefreshFeed() override;
  void RefreshFeed() override;
  void GetCurrentContent(GetCurrentContentCallback) override;
  void GetFeedProcessScopeDump(GetFeedProcessScopeDumpCallback) override;
  void GetFeedHistograms(GetFeedHistogramsCallback) override;
  void OverrideFeedHost(const GURL& host) override;
  void OverrideActionUploadEndpoint(const GURL& endpoint_url) override;

 private:
  bool IsFeedAllowed();

  mojo::Receiver<feed_internals::mojom::PageHandler> receiver_;

  // Services that provide the data and functionality.
  feed::FeedStreamApi* feed_stream_;
  PrefService* pref_service_;

  base::WeakPtrFactory<FeedV2InternalsPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEEDV2_INTERNALS_PAGE_HANDLER_H_
