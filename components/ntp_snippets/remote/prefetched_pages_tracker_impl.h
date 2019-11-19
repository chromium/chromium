// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_PREFETCHED_PAGES_TRACKER_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_PREFETCHED_PAGES_TRACKER_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/ntp_snippets/remote/prefetched_pages_tracker.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

namespace offline_pages {
struct OfflinePageItem;
}

namespace ntp_snippets {

// OfflinePageModel must outlive this class.
class PrefetchedPagesTrackerImpl
    : public PrefetchedPagesTracker,
      public offline_pages::OfflinePageModel::Observer {
 public:
  PrefetchedPagesTrackerImpl(
      offline_pages::OfflinePageModel* offline_page_model);
  ~PrefetchedPagesTrackerImpl() override;

  // PrefetchedPagesTracker implementation
  bool IsInitialized() const override;
  void Initialize(base::OnceCallback<void()> callback) override;
  bool PrefetchedOfflinePageExists(const GURL& url) const override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageAdded(
      offline_pages::OfflinePageModel* model,
      const offline_pages::OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const offline_pages::OfflinePageItem& deleted_page) override;

 private:
  void OfflinePagesLoaded(const std::vector<offline_pages::OfflinePageItem>&
                              all_prefetched_offline_pages);
  void AddOfflinePage(const offline_pages::OfflinePageItem& offline_page_item);

  bool initialized_;
  offline_pages::OfflinePageModel* offline_page_model_;

  // Mapping from an offline id to a URL for all currently known prefetched
  // offline pages.
  std::map<int64_t, GURL> offline_id_to_url_mapping_;
  // The mapping above represented as a mapping from a URL to its count. It does
  // not contain items with zero count.
  std::map<GURL, int> prefetched_url_counts_;

  std::vector<base::OnceCallback<void()>> initialization_completed_callbacks_;

  base::WeakPtrFactory<PrefetchedPagesTrackerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrefetchedPagesTrackerImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_PREFETCHED_PAGES_TRACKER_IMPL_H_
