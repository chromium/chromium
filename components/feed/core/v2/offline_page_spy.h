// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_OFFLINE_PAGE_SPY_H_
#define COMPONENTS_FEED_CORE_V2_OFFLINE_PAGE_SPY_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

namespace feed {
class SurfaceUpdater;

// Watches for availability of offline pages for pages linked on the Feed.
// Offline page availability is sent to
// |SurfaceUpdater::SetOfflinePageAvailability()|.
class OfflinePageSpy : public offline_pages::OfflinePageModel::Observer,
                       public feed::StreamModel::Observer {
 public:
  struct BadgeInfo {
    BadgeInfo();
    BadgeInfo(const GURL& url, const std::string& badge_id);
    // For sorting by (url, badge_id).
    bool operator<(const BadgeInfo& rhs) const;
    GURL url;
    std::string badge_id;
    // Initially, this is false until we receive information that indicates
    // otherwise.
    bool available_offline = false;
  };

  OfflinePageSpy(SurfaceUpdater* surface_updater,
                 offline_pages::OfflinePageModel* offline_page_model);
  ~OfflinePageSpy() override;
  OfflinePageSpy(const OfflinePageSpy&) = delete;
  OfflinePageSpy& operator=(const OfflinePageSpy&) = delete;

  void SetModel(StreamModel* stream_model);

 private:
  // offline_pages::OfflinePageModel::Observer
  void OfflinePageAdded(
      offline_pages::OfflinePageModel* model,
      const offline_pages::OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const offline_pages::OfflinePageItem& deleted_page) override;
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override {
  }

  // StreamModel::Observer
  void OnUiUpdate(const StreamModel::UiUpdate& update) override;

  void SetAvailability(const base::flat_set<GURL>& urls, bool available);
  void GetPagesDone(const std::vector<offline_pages::OfflinePageItem>& items);
  void UpdateWatchedPages();
  void RequestOfflinePageStatus(std::vector<GURL> new_urls);

  base::WeakPtr<OfflinePageSpy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  SurfaceUpdater* surface_updater_;                      // unowned
  offline_pages::OfflinePageModel* offline_page_model_;  // unowned
  // Null when the model is not loaded.
  StreamModel* stream_model_ = nullptr;  // unowned

  // A list of offline badges for all content in the stream.
  std::vector<BadgeInfo> badges_;

  base::WeakPtrFactory<OfflinePageSpy> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_OFFLINE_PAGE_SPY_H_
