// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_DRIVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_DRIVER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/history/core/browser/history_types.h"

class GURL;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace history {

class WebHistoryService;

// Interface serves as an abstraction layer BrowsingHistoryService and more
// platform logic and classes, facilitating both sending an receiving data.
class BrowsingHistoryDriver {
 public:
  BrowsingHistoryDriver(const BrowsingHistoryDriver&) = delete;
  BrowsingHistoryDriver& operator=(const BrowsingHistoryDriver&) = delete;

  // Callback for QueryHistory().
  virtual void OnQueryComplete(
      const std::vector<BrowsingHistoryService::HistoryEntry>& results,
      const BrowsingHistoryService::QueryResultsInfo& query_results_info,
      base::OnceClosure continuation_closure) {}

  // Callback for RemoveVisits().
  virtual void OnRemoveVisitsComplete() {}

  // Callback for RemoveVisits() that fails.
  virtual void OnRemoveVisitsFailed() {}

  // Callback for RemoveVisits() with the list of expire arguments. This gives
  // the driver a chance to perform embedder specific removal logic.
  virtual void OnRemoveVisits(
      const std::vector<ExpireHistoryArgs>& expire_list) = 0;

  // Called when `HistoryService` or `WebHistoryService` deletes one or more
  // items. But notably, this call is only used for deletions initiated by a
  // DIFFERENT tab.  Deletions initiated by the tab that owns this instance are
  // notified using `OnRemoveVisitsComplete()` or `OnRemoveVisitsFailed()`.
  // TODO(tommycli): Investigate consolidating the deletion callbacks.
  virtual void HistoryDeleted() {}

  // Whether other forms of browsing history were found on the history
  // service.
  virtual void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                              bool has_synced_results) {}

  // Fetch all the app IDs used in the database.
  virtual void OnGetAllAppIds(const std::vector<std::string>& app_ids) {}

  // If history deletions are currently allowed.
  virtual bool AllowHistoryDeletions() = 0;

  // If the given url from web history is allowed to be shown to the user.
  virtual bool ShouldHideWebHistoryUrl(const GURL& url) = 0;

  // Retrieve the WebHistory service, which may or may not currently exist or be
  // accessible.
  virtual WebHistoryService* GetWebHistoryService() = 0;

  // Whether the Clear Browsing Data UI should show a notice about the existence
  // of other forms of browsing history stored in user's account. The response
  // is returned in a `callback`.
  virtual void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      WebHistoryService* history_service,
      base::OnceCallback<void(bool)> callback) = 0;

 protected:
  BrowsingHistoryDriver() {}
  virtual ~BrowsingHistoryDriver() {}
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_DRIVER_H_
