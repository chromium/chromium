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

// Tracks several page-level (i.e. primary main frame document) Topics signals,
// such as whether the page is eligible to be included in topics calculation
// (i.e. to "observe" topics), the context domains that have requested to
// observe topics, the client side redirect chain's status, etc.
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
                               history::HistoryService* history_service,
                               bool observe);

  int redirect_count() const { return redirect_count_; }
  int redirect_with_topics_invoked_count() const {
    return redirect_with_topics_invoked_count_;
  }

  ukm::SourceId source_id_before_redirects() const {
    return source_id_before_redirects_;
  }

  bool topics_invoked() const { return topics_invoked_; }

 private:
  friend class PageUserData;

  explicit BrowsingTopicsPageLoadDataTracker(content::Page& page);

  BrowsingTopicsPageLoadDataTracker(content::Page& page,
                                    int redirect_count,
                                    int redirect_with_topics_invoked_count,
                                    ukm::SourceId source_id_before_redirects);

  // Whether this page is eligible to observe topics (i.e. IP was publicly
  // routable AND permissions policy is "allow").
  bool eligible_to_observe_ = false;

  HashedHost hashed_main_frame_host_;

  ukm::SourceId source_id_;

  base::flat_set<HashedDomain> observed_hashed_context_domains_;

  // The number of previous pages that were loaded via client-side redirects
  // (without user activation).
  int redirect_count_;

  // The number of previous pages that were loaded via client-side redirects
  // (without user activation) that also invoked the Topics API.
  int redirect_with_topics_invoked_count_;

  // The UKM source ID of the page before the client-side redirects.
  ukm::SourceId source_id_before_redirects_;

  bool topics_invoked_ = false;

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_PAGE_LOAD_DATA_TRACKER_H_
