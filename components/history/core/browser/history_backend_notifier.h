// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_

#include <set>

#include "components/history/core/browser/history_types.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace history {

// The HistoryBackendNotifier (mostly) forwards notifications from the
// HistoryBackend's client to all the interested observers (in both history
// and main thread).
class HistoryBackendNotifier {
 public:
  HistoryBackendNotifier() = default;
  virtual ~HistoryBackendNotifier() = default;

  // Sends notification that the favicons for the given page URLs (e.g.
  // http://www.google.com) and the given icon URL (e.g.
  // http://www.google.com/favicon.ico) have changed. It is valid to call
  // NotifyFaviconsChanged() with non-empty `page_urls` and an empty `icon_url`
  // and vice versa.
  virtual void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                     const GURL& icon_url) = 0;

  // Sends notification that a visit to `url_row` occurred with the details
  // (transition type, visit time, etc) given in `visit_row` and the associated
  // `local_navigation_id` from the underlying `content::NavigationHandle`,
  // which will be non-null only for navigations on the local device.
  // It is valid to call NotifyURLVisited() with an empty `local_navigation_id`.
  virtual void NotifyURLVisited(const URLRow& url_row,
                                const VisitRow& visit_row,
                                std::optional<int64_t> local_navigation_id) = 0;

  // Sends notification that `changed_urls` have been changed or added.
  virtual void NotifyURLsModified(const URLRows& changed_urls,
                                  bool is_from_expiration) = 0;

  // Sends notification that some or the totality of the URLs have been
  // deleted.
  // `deletion_info` describes the urls that have been removed from history.
  virtual void NotifyDeletions(DeletionInfo deletion_info) = 0;

  // Called after a visit has been updated.
  virtual void NotifyVisitUpdated(const VisitRow& visit,
                                  VisitUpdateReason reason) = 0;

  // Called after visits have been deleted. May also notify of any deleted
  // VisitedLinkRows as a result of the VisitRow deletion.
  virtual void NotifyVisitsDeleted(const std::vector<DeletedVisit>& visits) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_
