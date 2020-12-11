// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/history/profile_based_browsing_history_driver.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public content::WebUIMessageHandler,
                               public ProfileBasedBrowsingHistoryDriver {
 public:
  BrowsingHistoryHandler();
  ~BrowsingHistoryHandler() override;

  // WebUIMessageHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  void StartQueryHistory();

  // Handler for the "queryHistory" message.
  void HandleQueryHistory(const base::ListValue* args);

  // Handler for the "queryHistoryContinuation" message.
  void HandleQueryHistoryContinuation(const base::ListValue* args);

  // Handler for the "removeVisits" message.
  void HandleRemoveVisits(const base::ListValue* args);

  // Handler for "clearBrowsingData" message.
  void HandleClearBrowsingData(const base::ListValue* args);

  // Handler for "removeBookmark" message.
  void HandleRemoveBookmark(const base::ListValue* args);

  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override;
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void HistoryDeleted() override;
  void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                      bool has_synced_results) override;

  // ProfileBasedBrowsingHistoryDriver implementation.
  Profile* GetProfile() override;

  // For tests. This does not take the ownership of the clock. |clock| must
  // outlive the BrowsingHistoryHandler instance.
  void set_clock(base::Clock* clock) { clock_ = clock; }

 protected:
  virtual void SendHistoryQuery(int count, const base::string16& query);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest,
                           ObservingWebHistoryDeletions);
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest, MdTruncatesTitles);

  // The clock used to vend times.
  base::Clock* clock_;

  std::unique_ptr<history::BrowsingHistoryService> browsing_history_service_;

  std::vector<base::OnceClosure> deferred_callbacks_;

  base::Value initial_results_;

  std::string query_history_callback_id_;

  base::OnceClosure query_history_continuation_;

  std::string remove_visits_callback_;

  base::WeakPtrFactory<BrowsingHistoryHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_
