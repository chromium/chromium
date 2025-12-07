// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_DELEGATE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_DELEGATE_H_

#include <optional>

#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups {

class MockTabGroupSyncDelegate : public TabGroupSyncDelegate {
 public:
  MockTabGroupSyncDelegate();
  ~MockTabGroupSyncDelegate() override;

  MOCK_METHOD(std::optional<LocalTabGroupID>,
              HandleOpenTabGroupRequest,
              (const base::Uuid&, std::unique_ptr<TabGroupActionContext>));
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(void, CreateLocalTabGroup, (const SavedTabGroup&));
  MOCK_METHOD(void, ConnectLocalTabGroup, (const SavedTabGroup&));
  MOCK_METHOD(void, DisconnectLocalTabGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void, UpdateLocalTabGroup, (const SavedTabGroup&));
  MOCK_METHOD(void, CloseLocalTabGroup, (const LocalTabGroupID&));
  MOCK_METHOD(std::vector<LocalTabGroupID>, GetLocalTabGroupIds, ());
  MOCK_METHOD(std::vector<LocalTabID>,
              GetLocalTabIdsForTabGroup,
              (const LocalTabGroupID&));
  MOCK_METHOD(std::set<LocalTabID>, GetSelectedTabs, ());
  MOCK_METHOD(std::u16string, GetTabTitle, (const LocalTabID&));
  MOCK_METHOD(std::unique_ptr<SavedTabGroup>,
              CreateSavedTabGroupFromLocalGroup,
              (const LocalTabGroupID&));
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_TAB_GROUP_SYNC_DELEGATE_H_
