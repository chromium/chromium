// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_SERVICE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_SERVICE_H_

#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups {

class MockTabGroupSyncService : public TabGroupSyncService {
 public:
  MockTabGroupSyncService();
  ~MockTabGroupSyncService() override;

  MOCK_METHOD(void,
              SetTabGroupSyncDelegate,
              (std::unique_ptr<TabGroupSyncDelegate>));
  MOCK_METHOD(void, AddGroup, (SavedTabGroup));
  MOCK_METHOD(void, RemoveGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void, RemoveGroup, (const base::Uuid&));
  MOCK_METHOD(void,
              UpdateVisualData,
              (const LocalTabGroupID, const tab_groups::TabGroupVisualData*));
  MOCK_METHOD(void,
              UpdateGroupPosition,
              (const base::Uuid& sync_id,
               std::optional<bool> is_pinned,
               std ::optional<int> new_index));
  MOCK_METHOD(void,
              AddTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const std::u16string&,
               GURL,
               std::optional<size_t>));
  MOCK_METHOD(void,
              UpdateTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const SavedTabGroupTabBuilder&));
  MOCK_METHOD(void, RemoveTab, (const LocalTabGroupID&, const LocalTabID&));
  MOCK_METHOD(void, MoveTab, (const LocalTabGroupID&, const LocalTabID&, int));
  MOCK_METHOD(void, OnTabSelected, (const LocalTabGroupID&, const LocalTabID&));
  MOCK_METHOD(void, SaveGroup, (SavedTabGroup));
  MOCK_METHOD(void, UnsaveGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void,
              MakeTabGroupShared,
              (const LocalTabGroupID&, std::string_view));

  MOCK_METHOD(std::vector<SavedTabGroup>, GetAllGroups, ());
  MOCK_METHOD(std::optional<SavedTabGroup>, GetGroup, (const base::Uuid&));
  MOCK_METHOD(std::optional<SavedTabGroup>, GetGroup, (const LocalTabGroupID&));
  MOCK_METHOD(std::vector<LocalTabGroupID>, GetDeletedGroupIds, ());

  MOCK_METHOD(void,
              OpenTabGroup,
              (const base::Uuid&, std::unique_ptr<TabGroupActionContext>));
  MOCK_METHOD(void,
              UpdateLocalTabGroupMapping,
              (const base::Uuid&, const LocalTabGroupID&, OpeningSource));
  MOCK_METHOD(void,
              RemoveLocalTabGroupMapping,
              (const LocalTabGroupID&, ClosingSource));
  MOCK_METHOD(void,
              UpdateLocalTabId,
              (const LocalTabGroupID&, const base::Uuid&, const LocalTabID&));
  MOCK_METHOD(void,
              ConnectLocalTabGroup,
              (const base::Uuid&, const LocalTabGroupID&, OpeningSource));
  MOCK_METHOD(bool,
              IsRemoteDevice,
              (const std::optional<std::string>&),
              (const));
  MOCK_METHOD(bool,
              WasTabGroupClosedLocally,
              (const base::Uuid& sync_id),
              (const));
  MOCK_METHOD(void, RecordTabGroupEvent, (const EventDetails&));
  MOCK_METHOD(TabGroupSyncMetricsLogger*, GetTabGroupSyncMetricsLogger, ());

  MOCK_METHOD(syncer::DataTypeSyncBridge*, bridge, ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSavedTabGroupControllerDelegate,
              ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSharedTabGroupControllerDelegate,
              ());
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(void,
              GetURLRestriction,
              (const GURL&, TabGroupSyncService::UrlRestrictionCallback));

  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_SERVICE_H_
