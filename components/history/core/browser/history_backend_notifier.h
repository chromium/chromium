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
  // (transition type, visit time, etc) given in `visit_row`.
  virtual void NotifyURLVisited(const URLRow& url_row,
                                const VisitRow& visit_row) = 0;

  // Sends notification that `changed_urls` have been changed or added.
  virtual void NotifyURLsModified(const URLRows& changed_urls,
                                  bool is_from_expiration) = 0;

  // Sends notification that some or the totality of the URLs have been
  // deleted.
  // `deletion_info` describes the urls that have been removed from history.
  virtual void NotifyURLsDeleted(DeletionInfo deletion_info) = 0;

  // Called after a visit has been updated.
  virtual void NotifyVisitUpdated(const VisitRow& visit) = 0;

  // Called after a visit has been deleted.
  virtual void NotifyVisitDeleted(const VisitRow& visit) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_
