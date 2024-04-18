// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/search/search_provider_observer.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/sync/service/sync_service.h"

namespace content {
class WebUI;
}

namespace settings {

// Chrome browser startup settings handler.
class ClearBrowsingDataHandler : public SettingsPageUIHandler,
                                 public syncer::SyncServiceObserver,
                                 public TemplateURLServiceObserver {
 public:
  ClearBrowsingDataHandler(content::WebUI* webui, Profile* profile);

  ClearBrowsingDataHandler(const ClearBrowsingDataHandler&) = delete;
  ClearBrowsingDataHandler& operator=(const ClearBrowsingDataHandler&) = delete;

  ~ClearBrowsingDataHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Calls |HandleClearBrowsingData| with test data for browser test.
  void HandleClearBrowsingDataForTest();

 private:
  friend class TestingClearBrowsingDataHandler;
  friend class ClearBrowsingDataHandlerUnitTest;
  FRIEND_TEST_ALL_PREFIXES(ClearBrowsingDataHandlerUnitTest,
                           UpdateSyncState_GoogleDse);
  FRIEND_TEST_ALL_PREFIXES(ClearBrowsingDataHandlerUnitTest,
                           UpdateSyncState_NonGoogleDsePrepopulated);
  FRIEND_TEST_ALL_PREFIXES(ClearBrowsingDataHandlerUnitTest,
                           UpdateSyncState_NonGoogleDseNotPrepopulated);

  // Clears browsing data, called by Javascript.
  void HandleClearBrowsingData(const base::Value::List& value);

  // Called when a clearing task finished. |webui_callback_id| is provided
  // by the WebUI action that initiated it.
  // The ScopedSyncedDataDeletion is passed here to ensure that the Sync token
  // is not invalidated before this function is run.
  void OnClearingTaskFinished(
      const std::string& webui_callback_id,
      const base::flat_set<browsing_data::BrowsingDataType>& data_types,
      std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion,
      uint64_t failed_data_types);

  // Initializes the dialog UI. Called by JavaScript when the DOM is ready.
  void HandleInitialize(const base::Value::List& args);

  // Returns the current sync state to the WebUI.
  void HandleGetSyncState(const base::Value::List& args);

  // Called by WebUI when the user takes an action that warrants restarting
  // counters.
  // TODO(crbug.com/331925113): Currently, this only happens when the time
  // range dropdown is changed. However, it would make sense to also restart
  // timers when a checkbox state changes. If that's not the case, this method
  // should be reconciled with `HandleTimePeriodChanged` below which likewise
  // triggers on the dropdown change, but only after the deletion has been
  // executed and prefs updated.
  void HandleRestartCounters(const base::Value::List& args);

  // Implementation of SyncServiceObserver.
  void OnStateChanged(syncer::SyncService* sync) override;

  // Updates the footer of the dialog when the sync state changes.
  virtual void UpdateSyncState();

  // Create a SyncStateEvent containing the current sync state.
  base::Value::Dict CreateSyncStateEvent();

  // Finds out whether we should show notice about other forms of history stored
  // in user's account.
  void RefreshHistoryNotice();

  // Called as an asynchronous response to |RefreshHistoryNotice()|. Enables or
  // disables the dialog about other forms of history stored in user's account
  // that is shown when the history deletion is finished.
  void UpdateHistoryDeletionDialog(bool show);

  // Adds a browsing data |counter|.
  void AddCounter(std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
                  browsing_data::ClearBrowsingDataTab tab);

  // Updates a counter text according to the |result|.
  void UpdateCounterText(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);

  // Restarts |counters_basic_| or |counters_advanced_| depending on the |basic|
  // argument, and instructs them to calculate the data volume for
  // the |time_period|.
  void RestartCounters(bool basic, browsing_data::TimePeriod time_period);

  // Record changes to the time period preferences.
  void HandleTimePeriodChanged(const std::string& pref_name);

  // Implementation of TemplateURLServiceObserver.
  void OnTemplateURLServiceChanged() override;

  // Cached profile corresponding to the WebUI of this handler.
  raw_ptr<Profile> profile_;

  // Counters that calculate the data volume for individual data types.
  std::vector<std::unique_ptr<browsing_data::BrowsingDataCounter>>
      counters_basic_;
  std::vector<std::unique_ptr<browsing_data::BrowsingDataCounter>>
      counters_advanced_;

  // SyncService to observe sync state changes.
  raw_ptr<syncer::SyncService> sync_service_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      dse_service_observation_{this};

  // Whether we should show a dialog informing the user about other forms of
  // history stored in their account after the history deletion is finished.
  bool show_history_deletion_dialog_;

  // A weak pointer factory for asynchronous calls referencing this class.
  // The weak pointers are invalidated in |OnJavascriptDisallowed()| and
  // |HandleInitialize()| to cancel previously initiated tasks.
  base::WeakPtrFactory<ClearBrowsingDataHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
