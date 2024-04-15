// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import androidx.annotation.NonNull;

import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

/**
 * The core service class for handling tab group sync across devices. Provides 1. Mutation methods
 * to propagate local changes to remote. 2. Observer interface to propagate remote changes to local.
 */
public interface TabGroupSyncService {
    /**
     * Observers observing updates to the sync data which can be originated by either the local or
     * remote clients.
     */
    interface Observer {
        /**
         * Called when the sync database (ModelTypeStore) has been initialized and fully loaded to
         * memory.
         */
        void onInitialized();

        /**
         * Called when a new tab group was added from sync. A corresponding local tab group should
         * be created.
         *
         * @param group The {@link SavedTabGroup} that was added from sync.
         */
        void onTabGroupAdded(SavedTabGroup group);

        /**
         * Called when a tab group is updated from sync. The corresponding local tab group should be
         * updated to match the sync representation.
         *
         * @param group The {@link SavedTabGroup} that was updated from sync.
         */
        void onTabGroupUpdated(SavedTabGroup group);

        /**
         * Called when a tab group is deleted from sync. The local tab group should be deleted in
         * response and all the corresponding tabs should be closed.
         *
         * @param localId The local ID corresponding to the tab group that was removed.
         */
        void onTabGroupRemoved(int localId);

        /**
         * Called when a tab group is deleted from sync. This signal is used by revisit surface that
         * needs to show both open and closed tab groups. All other consumers should use the local
         * ID variant of this signal.
         *
         * @param syncId The sync ID corresponding to the tab group that was removed.
         */
        void onTabGroupRemoved(String syncId);
    }

    /**
     * Add an observer to be notified of sync changes.
     *
     * @param observer The observer to be notified.
     */
    void addObserver(Observer observer);

    /**
     * Remove a given observer.
     *
     * @param observer The observer to be removed.
     */
    void removeObserver(Observer observer);

    /**
     * Creates a remote tab group with the given local group ID.
     *
     * @param groupId The local group ID.
     * @return The sync ID of the group after it has been added to sync.
     */
    String createGroup(int groupId);

    /**
     * Removes a remote tab group in response to a local group removal.
     *
     * @param groupId The ID of the tab group removed. Currently this is the root ID of the group.
     */
    void removeGroup(int groupId);

    /**
     * Updates the visual info of the remote tab group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param title The title of the tab group.
     * @param color The color of the tab group.
     */
    void updateVisualData(int tabGroupId, @NonNull String title, @TabGroupColorId int color);

    /**
     * Adds a tab to a remote group. Should be called with response to a local tab addition to a tab
     * group. If position is -1, adds the tab to the end of the group.
     *
     * @param tabGroupId The local ID of the corresponding tab group.
     * @param tabId The local tab ID of the tab being added.
     * @param title The title of the tab.
     * @param url The URL of the tab.
     * @param position The position in the remote tab group at which the tab should be inserted. If
     *     -1, inserts it at the end of the group.
     */
    void addTab(int tabGroupId, int tabId, String title, GURL url, int position);

    /**
     * Updates a remote tab corresponding to local tab ID {@param tabId}.
     *
     * @param tabGroupId The local ID of the corresponding tab group.
     * @param tabId The local tab ID of the tab being added.
     * @param title The title of the tab.
     * @param url The URL of the tab.
     * @param position The position in the remote tab group at which the tab should be inserted. If
     *     -1, inserts it at the end of the group.
     */
    void updateTab(int tabGroupId, int tabId, String title, GURL url, int position);

    /**
     * Removes a tab from the remote tab group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param tabId The local tab ID of the tab being removed.
     */
    void removeTab(int tabGroupId, int tabId);

    /**
     * Called to return all the remote tab group IDs currently existing in the system.
     *
     * @return An array of IDs of the currently known tab groups.
     */
    String[] getAllGroupIds();

    /**
     * Returns a single {@link SavedTabGroup}.
     *
     * @param syncGroupId The sync ID of the group to be returned.
     * @return The associated {@link SavedTabGroup}.
     */
    SavedTabGroup getGroup(String syncGroupId);

    /**
     * Returns a single {@link SavedTabGroup}.
     *
     * @param localGroupId The local ID of the group to be returned.
     * @return The associated {@link SavedTabGroup}.
     */
    SavedTabGroup getGroup(int localGroupId);

    /**
     * Updates the in-memory mapping between sync and local tab group IDs.
     *
     * @param syncId The remote tab group ID.
     * @param localId The local tab group ID.
     */
    void updateLocalTabGroupId(String syncId, int localId);

    /**
     * Updates the in-memory mapping between sync and local IDs for a given tab.
     *
     * @param localGroupId The local group ID of the corresponding tab group.
     * @param syncTabId The sync ID of the corresponding tab.
     * @param localTabId The local ID of the corresponding tab.
     */
    void updateLocalTabId(int localGroupId, String syncTabId, int localTabId);
}
