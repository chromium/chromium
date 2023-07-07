// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_PAGE_LOAD_DATA_TRACKER_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_PAGE_LOAD_DATA_TRACKER_H_

#include "base/containers/flat_set.h"
#include "components/browsing_topics/common/common_types.h"
#include "content/public/browser/page_user_data.h"

namespace history {
class HistoryService;
}  // namespace history

namespace browsing_topics {

// Tracks page-level (i.e. primary main frame document) signals to determine
// whether the page is eligible to be included in browsing topics calculation.
// Also tracks the context domains that have used the Topics API in the page.
class BrowsingTopicsPageLoadDataTracker
    : public content::PageUserData<BrowsingTopicsPageLoadDataTracker> {
 public:
  BrowsingTopicsPageLoadDataTracker(const BrowsingTopicsPageLoadDataTracker&) =
      delete;
  BrowsingTopicsPageLoadDataTracker& operator=(
      const BrowsingTopicsPageLoadDataTracker&) = delete;

  ~BrowsingTopicsPageLoadDataTracker() override;

  // Called when the document.browsingTopics() API is used in the page.
  void OnBrowsingTopicsApiUsed(const HashedDomain& hashed_context_domain,
                               const std::string& context_domain,
                               history::HistoryService* history_service);

 private:
  friend class PageUserData;

  explicit BrowsingTopicsPageLoadDataTracker(content::Page& page);

  // |eligible_to_commit_| means all the commit time prerequisites are met
  // (i.e. IP was publicly routable AND permissions policy is "allow").
  bool eligible_to_commit_ = false;

  HashedHost hashed_main_frame_host_;

  ukm::SourceId source_id_;

  base::flat_set<HashedDomain> observed_hashed_context_domains_;

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_PAGE_LOAD_DATA_TRACKER_H_
