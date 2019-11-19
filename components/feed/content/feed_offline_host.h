// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CONTENT_FEED_OFFLINE_HOST_H_
#define COMPONENTS_FEED_CONTENT_FEED_OFFLINE_HOST_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "components/feed/core/content_metadata.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"

class GURL;

namespace offline_pages {
class PrefetchService;
}  // namespace offline_pages

namespace feed {

// Responsible for wiring up connections for Feed operations pertaining to
// articles that can be loaded from Offline Pages component. Most significantly
// this class connects Prefetch and the Feed, and tracks offlined articles the
// Feed may have badged for this user. This knowledge is later used when Feed
// articles are opened to populate load params.
class FeedOfflineHost : public offline_pages::SuggestionsProvider,
                        public offline_pages::OfflinePageModel::Observer {
 public:
  using GetKnownContentCallback =
      base::OnceCallback<void(std::vector<ContentMetadata>)>;
  using NotifyStatusChangeCallback =
      base::RepeatingCallback<void(const std::string&, bool)>;

  FeedOfflineHost(offline_pages::OfflinePageModel* offline_page_model,
                  offline_pages::PrefetchService* prefetch_service,
                  base::RepeatingClosure on_suggestion_consumed,
                  base::RepeatingClosure on_suggestions_shown);
  ~FeedOfflineHost() override;

  // Initialize with callbacks to call into bridge/Java side. Should only be
  // called once, and done as soon as the bridge is ready. The FeedOfflineHost
  // will not be fully ready to perform its function without these dependencies.
  // Neither of these callbacks will be invoked until after this method exits.
  void Initialize(const base::RepeatingClosure& trigger_get_known_content,
                  const NotifyStatusChangeCallback& notify_status_change);

  // Called during initialization make ourselves known to |prefetch_service_|.
  // This method is used to wrap PrefetchService::SetSuggestionProvider() to let
  // our weak pointer guarantee everyone is still alive.
  void SetSuggestionProvider();

  // Synchronously returns the offline id of the given page. The host will only
  // have knowledge of the page if it had previously returned status about it
  // through GetOfflineState() or as a notification. Otherwise the caller will
  // receive a false negative. Additionally, since the host tracks pages by
  // hashing, there's also a small chance that the host erroneously returns an
  // id for a page that is not offlined.
  base::Optional<int64_t> GetOfflineId(const std::string& url);

  // Asynchronously fetches offline status for the given URLs. Any pages that
  // are currently offlined will be remembered by the FeedOfflineHost.
  void GetOfflineStatus(
      std::vector<std::string> urls,
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Should be called from Feed any time the user manually removes articles or
  // groupings of articles. Propagates the signal to Prefetch.
  void OnContentRemoved(std::vector<std::string> urls);

  // Should be called from Feed any time new articles are fetched.
  void OnNewContentReceived();

  // Should be called from Feed side any time there are no active surfaces
  // displaying articles and listening to our notifications. This signal is used
  // to clear local tracking of offlined items.
  void OnNoListeners();

  // Should be called when async GetKnownContent is completed. Broadcasts to all
  // waiting consumers in |pending_known_content_callbacks_|.
  void OnGetKnownContentDone(std::vector<ContentMetadata> suggestions);

  // offline_pages::SuggestionsProvider:
  void GetCurrentArticleSuggestions(
      offline_pages::SuggestionsProvider::SuggestionCallback
          suggestions_callback) override;
  void ReportArticleListViewed() override;
  void ReportArticleViewed(GURL article_url) override;

  // offline_pages::OfflinePageModel::Observer:
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageAdded(
      offline_pages::OfflinePageModel* model,
      const offline_pages::OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const offline_pages::OfflinePageItem& deleted_page) override;

 private:
  // Stores the given record in |url_hash_to_id_|. If there's a conflict, the
  // new id will overwrite the old value.
  void CacheOfflinePageUrlAndId(const std::string& url, int64_t id);

  // Removes a previously cached |id| for the given |url| if there was one.
  void EvictOfflinePageUrl(const std::string& url);

  // The following objects all outlive us, so it is safe to hold raw pointers to
  // them. This is guaranteed by the FeedHostServiceFactory.
  offline_pages::OfflinePageModel* offline_page_model_;
  offline_pages::PrefetchService* prefetch_service_;

  base::RepeatingClosure on_suggestion_consumed_;
  base::RepeatingClosure on_suggestions_shown_;

  // Only offlined pages that have passed through the host are stored. If there
  // are ever no listeners to the offline host logic and OnNoListeners() is
  // called this map is cleared. The key is the hash of the url, and the value
  // is the offline id for the given page.
  base::flat_map<size_t, int64_t> url_hash_to_id_;

  // Starts an the async request for ContentMetadata through KnownContentApi's
  // GetKnownContent(). Will only be invoked when there isn't already an
  // outstanding GetKnownContent().
  base::RepeatingClosure trigger_get_known_content_;

  // Holds all consumers of GetKnownContent(). It is assumed that there's an
  // outstanding GetKnownContent() if and only if this vector is not empty.
  std::vector<SuggestionsProvider::SuggestionCallback>
      pending_known_content_callbacks_;

  // Calls all OfflineStatusListeners with the updated status.
  NotifyStatusChangeCallback notify_status_change_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FeedOfflineHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedOfflineHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CONTENT_FEED_OFFLINE_HOST_H_
