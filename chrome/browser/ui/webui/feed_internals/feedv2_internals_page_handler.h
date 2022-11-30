// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEEDV2_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEEDV2_INTERNALS_PAGE_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefService;
namespace feed {
class FeedService;
class FeedApi;
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
  void GetLastFetchProperties(GetLastFetchPropertiesCallback) override;
  void RefreshForYouFeed() override;
  void RefreshFollowingFeed() override;
  void RefreshWebFeedSuggestions() override;
  void GetFeedProcessScopeDump(GetFeedProcessScopeDumpCallback) override;
  void GetFeedHistograms(GetFeedHistogramsCallback) override;
  void OverrideFeedHost(const GURL& host) override;
  void OverrideDiscoverApiEndpoint(const GURL& endpoint_url) override;
  void OverrideFeedStreamData(const std::vector<uint8_t>& data) override;
  void SetWebFeedFollowIntroDebugEnabled(const bool enabled) override;
  void SetUseFeedQueryRequests(const bool use_legacy) override;
  void SetFollowingFeedOrder(
      const feed_internals::mojom::FeedOrder new_order) override;

 private:
  bool IsFeedAllowed();
  bool IsWebFeedFollowIntroDebugEnabled();
  bool ShouldUseFeedQueryRequests();
  feed_internals::mojom::FeedOrder GetFollowingFeedOrder();

  mojo::Receiver<feed_internals::mojom::PageHandler> receiver_;

  // Services that provide the data and functionality.
  raw_ptr<feed::FeedApi> feed_stream_;
  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<FeedV2InternalsPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEEDV2_INTERNALS_PAGE_HANDLER_H_
