// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefService;

namespace feed {
class FeedHostService;
class FeedOfflineHost;
class FeedSchedulerHost;
}  // namespace feed

namespace offline_pages {
struct PrefetchSuggestion;
}  // namespace offline_pages

// Concrete implementation of feed_internals::mojom::PageHandler.
class FeedInternalsPageHandler : public feed_internals::mojom::PageHandler {
 public:
  FeedInternalsPageHandler(
      mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver,
      feed::FeedHostService* feed_host_service,
      PrefService* pref_service);
  ~FeedInternalsPageHandler() override;

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
  void OverrideFeedHost(const std::string& host) override;

 private:
  mojo::Receiver<feed_internals::mojom::PageHandler> receiver_;

  void OnGetCurrentArticleSuggestionsDone(
      GetCurrentContentCallback callback,
      std::vector<offline_pages::PrefetchSuggestion> suggestions);

  bool IsFeedAllowed();

  // Services that provide the data and functionality.
  feed::FeedSchedulerHost* feed_scheduler_host_;
  feed::FeedOfflineHost* feed_offline_host_;
  PrefService* pref_service_;

  base::WeakPtrFactory<FeedInternalsPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_
