// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_IMPL_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_helper.h"

class PrefService;
class TabRestoreServiceImplTest;

namespace sessions {

// Tab restore service that persists data on disk.
class SESSIONS_EXPORT TabRestoreServiceImpl : public TabRestoreService {
 public:
  // Does not take ownership of |time_factory|.
  TabRestoreServiceImpl(std::unique_ptr<TabRestoreServiceClient> client,
                        PrefService* pref_service,
                        tab_restore::TimeFactory* time_factory);

  TabRestoreServiceImpl(const TabRestoreServiceImpl&) = delete;
  TabRestoreServiceImpl& operator=(const TabRestoreServiceImpl&) = delete;

  ~TabRestoreServiceImpl() override;

  // TabRestoreService:
  void AddObserver(TabRestoreServiceObserver* observer) override;
  void RemoveObserver(TabRestoreServiceObserver* observer) override;
  std::optional<SessionID> CreateHistoricalTab(LiveTab* live_tab,
                                               int index) override;
  void BrowserClosing(LiveTabContext* context) override;
  void BrowserClosed(LiveTabContext* context) override;
  void CreateHistoricalGroup(LiveTabContext* context,
                             const tab_groups::TabGroupId& id) override;
  void GroupClosed(const tab_groups::TabGroupId& group) override;
  void GroupCloseStopped(const tab_groups::TabGroupId& group) override;
  void ClearEntries() override;
  void DeleteNavigationEntries(const DeletionPredicate& predicate) override;
  const Entries& entries() const override;
  std::vector<LiveTab*> RestoreMostRecentEntry(
      LiveTabContext* context) override;
  void RemoveEntryById(SessionID id) override;
  std::vector<LiveTab*> RestoreEntryById(
      LiveTabContext* context,
      SessionID id,
      WindowOpenDisposition disposition) override;
  void LoadTabsFromLastSession() override;
  bool IsLoaded() const override;
  void DeleteLastSession() override;
  bool IsRestoring() const override;
  void Shutdown() override;

  void CreateRestoredEntryCommandForTest(SessionID id);

 private:
  friend class ::TabRestoreServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(TabRestoreTest,
                           RestoreGroupInBrowserThatDoesNotSupportGroups);

  class PersistenceDelegate;
  void UpdatePersistenceDelegate();

  // Exposed for testing.
  Entries* mutable_entries();
  void PruneEntries();

  std::unique_ptr<TabRestoreServiceClient> client_;
  std::unique_ptr<PersistenceDelegate> persistence_delegate_;
  TabRestoreServiceHelper helper_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_IMPL_H_
