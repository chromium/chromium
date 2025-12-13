// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_SERVICE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_SERVICE_H_

#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/sync/model/data_type_sync_bridge.h"
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
              UpdateBookmarkNodeId,
              (const base::Uuid&, std::optional<base::Uuid>));
  MOCK_METHOD(void,
              AddTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const std::u16string&,
               const GURL&,
               std::optional<size_t>));
  MOCK_METHOD(void,
              AddUrl,
              (const base::Uuid&, const std::u16string&, const GURL&));
  MOCK_METHOD(void,
              NavigateTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const GURL&,
               const std::u16string&));
  MOCK_METHOD(void,
              UpdateTabProperties,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const SavedTabGroupTabBuilder&));
  MOCK_METHOD(void, RemoveTab, (const LocalTabGroupID&, const LocalTabID&));
  MOCK_METHOD(void, MoveTab, (const LocalTabGroupID&, const LocalTabID&, int));
  MOCK_METHOD(void,
              OnTabSelected,
              (const std::optional<LocalTabGroupID>&,
               const LocalTabID&,
               const std::u16string&));
  MOCK_METHOD(void, SaveGroup, (SavedTabGroup));
  MOCK_METHOD(void, UnsaveGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void,
              MakeTabGroupShared,
              (const LocalTabGroupID&,
               const syncer::CollaborationId&,
               TabGroupSharingCallback));
  MOCK_METHOD(void,
              MakeTabGroupSharedForTesting,
              (const LocalTabGroupID&, const syncer::CollaborationId&));
  MOCK_METHOD(void, MakeTabGroupUnsharedForTesting, (const LocalTabGroupID&));
  MOCK_METHOD(void,
              AboutToUnShareTabGroup,
              (const LocalTabGroupID&, base::OnceClosure));
  MOCK_METHOD(void, OnTabGroupUnShareComplete, (const LocalTabGroupID&, bool));
  MOCK_METHOD(void, OnCollaborationRemoved, (const syncer::CollaborationId&));

  MOCK_METHOD(std::vector<const SavedTabGroup*>, ReadAllGroups, (), (const));
  MOCK_METHOD(std::vector<SavedTabGroup>, GetAllGroups, (), (const));
  MOCK_METHOD(std::optional<SavedTabGroup>,
              GetGroup,
              (const base::Uuid&),
              (const));
  MOCK_METHOD(std::optional<SavedTabGroup>,
              GetGroup,
              (const LocalTabGroupID&),
              (const));
  MOCK_METHOD(std::optional<SavedTabGroup>,
              GetGroup,
              (const EitherGroupID&),
              (const));
  MOCK_METHOD(std::vector<LocalTabGroupID>, GetDeletedGroupIds, (), (const));
  MOCK_METHOD(std::optional<std::u16string>,
              GetTitleForPreviouslyExistingSharedTabGroup,
              (const syncer::CollaborationId&),
              (const));

  MOCK_METHOD(std::optional<LocalTabGroupID>,
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
  MOCK_METHOD(void, UpdateArchivalStatus, (const base::Uuid&, bool));
  MOCK_METHOD(void,
              UpdateTabLastSeenTime,
              (const base::Uuid&, const base::Uuid&, TriggerSource));
  MOCK_METHOD(TabGroupSyncMetricsLogger*, GetTabGroupSyncMetricsLogger, ());

  MOCK_METHOD(syncer::DataTypeSyncBridge*, bridge, ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSavedTabGroupControllerDelegate,
              ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSharedTabGroupControllerDelegate,
              ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSharedTabGroupAccountControllerDelegate,
              ());
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(void,
              GetURLRestriction,
              (const GURL&, TabGroupSyncService::UrlRestrictionCallback));
  MOCK_METHOD(std::unique_ptr<std::vector<SavedTabGroup>>,
              TakeSharedTabGroupsAvailableAtStartupForMessaging,
              ());
  MOCK_METHOD(bool, HadSharedTabGroupsLastSession, (bool), (override));
  MOCK_METHOD(VersioningMessageController*,
              GetVersioningMessageController,
              (),
              (override));
  MOCK_METHOD(void, OnLastTabClosed, (const SavedTabGroup&));

  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_SERVICE_H_
