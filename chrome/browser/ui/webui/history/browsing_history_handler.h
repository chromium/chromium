// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/history/profile_based_browsing_history_driver.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"

namespace content {
class WebContents;
}  // namespace content

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public history::mojom::PageHandler,
                               public ProfileBasedBrowsingHistoryDriver {
 public:
  BrowsingHistoryHandler(
      mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);

  BrowsingHistoryHandler(const BrowsingHistoryHandler&) = delete;
  BrowsingHistoryHandler& operator=(const BrowsingHistoryHandler&) = delete;

  ~BrowsingHistoryHandler() override;

  void SetSidePanelUIEmbedder(
      base::WeakPtr<TopChromeWebUIController::Embedder> side_panel_embedder);

  void StartQueryHistory();

  void SetPage(mojo::PendingRemote<history::mojom::Page> pending_page) override;

  void ShowSidePanelUI() override;

  void QueryHistory(const std::string& query,
                    int max_count,
                    std::optional<double> begin_timestamp,
                    QueryHistoryCallback callback) override;

  void QueryHistoryContinuation(
      QueryHistoryContinuationCallback callback) override;

  void RemoveVisits(const std::vector<history::mojom::RemovalItemPtr> items,
                    RemoveVisitsCallback callback) override;

  void OpenClearBrowsingDataDialog() override;

  void RemoveBookmark(const std::string& url) override;

  void SetLastSelectedTab(const int last_tab) override;

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

  void set_browsing_history_service_for_testing(
      std::unique_ptr<history::BrowsingHistoryService> service) {
    browsing_history_service_ = std::move(service);
  }

  history::BrowsingHistoryService* get_browsing_history_service_for_testing() {
    return browsing_history_service_.get();
  }

 protected:
  virtual void SendHistoryQuery(int count,
                                const std::string& query,
                                std::optional<double> begin_timestamp);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest,
                           ObservingWebHistoryDeletions);
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest, MdTruncatesTitles);

  base::WeakPtr<TopChromeWebUIController::Embedder> side_panel_embedder_;

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  mojo::Remote<history::mojom::Page> page_;
  mojo::Receiver<history::mojom::PageHandler> page_handler_;

  // The clock used to vend times.
  raw_ptr<base::Clock> clock_;

  std::unique_ptr<history::BrowsingHistoryService> browsing_history_service_;

  std::vector<base::OnceClosure> deferred_callbacks_;

  QueryHistoryCallback query_history_callback_;

  base::OnceClosure query_history_continuation_;

  std::queue<RemoveVisitsCallback> remove_visits_callbacks_;

  base::WeakPtrFactory<BrowsingHistoryHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_
