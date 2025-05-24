// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.when;

import org.mockito.ArgumentMatcher;

import org.chromium.base.Token;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * Test helpers for creating tab_group_sync objects and mocking calls to {@link
 * TabGroupSyncService}.
 */
public class SyncedGroupTestHelper {
    public static final String SYNC_GROUP_ID1 = "syncId1";
    public static final String SYNC_GROUP_ID2 = "syncId2";
    public static final String SYNC_GROUP_ID3 = "syncId3";

    /** Returns a list of empty tab objects. */
    public static List<SavedTabGroupTab> tabsFromCount(int count) {
        List<SavedTabGroupTab> tabList = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            tabList.add(new SavedTabGroupTab());
        }
        return tabList;
    }

    /** Returns a list of tabs with the given urls set. */
    public static List<SavedTabGroupTab> tabsFromUrls(GURL... gurls) {
        List<SavedTabGroupTab> tabList = new ArrayList<>();
        for (int i = 0; i < gurls.length; i++) {
            SavedTabGroupTab tab = new SavedTabGroupTab();
            tab.url = gurls[i];
            tab.localId = i;
            tabList.add(tab);
        }
        return tabList;
    }

    /** Returns a list of tabs with the given ids set. */
    public static List<SavedTabGroupTab> tabsFromIds(int... tabIds) {
        List<SavedTabGroupTab> tabList = new ArrayList<>();
        for (int tabId : tabIds) {
            SavedTabGroupTab tab = new SavedTabGroupTab();
            tab.localId = tabId;
            tabList.add(tab);
        }
        return tabList;
    }

    private final TabGroupSyncService mTabGroupSyncService;
    private final Set<String> mTabGroupSyncIdSet = new HashSet<>();

    /**
     * @param mockTabGroupSyncService The mocked service to set expectations with.
     */
    public SyncedGroupTestHelper(TabGroupSyncService mockTabGroupSyncService) {
        mTabGroupSyncService = mockTabGroupSyncService;
        when(mTabGroupSyncService.getAllGroupIds())
                .thenAnswer(ignored -> mTabGroupSyncIdSet.toArray(String[]::new));
    }

    /**
     * @param syncId The sync id of the group, when not unique it will overwrite previous mocks.
     * @return A new group object.
     */
    public SavedTabGroup newTabGroup(String syncId) {
        mTabGroupSyncIdSet.add(syncId);
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = syncId;
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(group);
        return group;
    }

    /**
     * @param syncId The sync id of the group, when not unique it will overwrite previous mocks.
     * @param tabGroupId The local id of the group.
     * @return A new group object.
     */
    public SavedTabGroup newTabGroup(String syncId, Token tabGroupId) {
        SavedTabGroup group = newTabGroup(syncId);
        group.localId = new LocalTabGroupId(tabGroupId);
        when(mTabGroupSyncService.getGroup(argThat(matchesTabGroupToken(tabGroupId))))
                .thenReturn(group);
        return group;
    }

    /** Removes the tab group from mocked responses. */
    public void removeTabGroup(String syncId) {
        mTabGroupSyncIdSet.remove(syncId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(null);
    }

    private ArgumentMatcher<LocalTabGroupId> matchesTabGroupToken(Token tabGroupId) {
        return localTabGroupId ->
                localTabGroupId != null && Objects.equals(localTabGroupId.tabGroupId, tabGroupId);
    }
}
