// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.List;

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
         * Called when the sync database (DataTypeStore) has been initialized and fully loaded to
         * memory.
         */
        default void onInitialized() {}

        /**
         * Called when a new tab group was added from sync. A corresponding local tab group should
         * be created.
         *
         * @param group The {@link SavedTabGroup} that was added from sync.
         * @param source The source of the event which can be local or remote.
         */
        default void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {}

        /**
         * Called when a tab group is updated from sync. The corresponding local tab group should be
         * updated to match the sync representation.
         *
         * @param group The {@link SavedTabGroup} that was updated from sync.
         * @param source The source of the event which can be local or remote.
         */
        default void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {}

        /**
         * Called when a tab group is deleted from sync. The local tab group should be deleted in
         * response and all the corresponding tabs should be closed.
         *
         * @param localTabGroupId The local ID corresponding to the tab group that was removed.
         * @param source The source of the event which can be local or remote.
         */
        default void onTabGroupRemoved(
                LocalTabGroupId localTabGroupId, @TriggerSource int source) {}

        /**
         * Called when a tab group is deleted from sync. This signal is used by revisit surface that
         * needs to show both open and closed tab groups. All other consumers should use the local
         * ID variant of this signal.
         *
         * @param syncTabGroupId The sync ID corresponding to the tab group that was removed.
         * @param source The source of the event which can be local or remote.
         */
        default void onTabGroupRemoved(String syncTabGroupId, @TriggerSource int source) {}

        /**
         * Called when the local ID for a tab group changes. Since Android uses stable tab group ID,
         * this is only fired when the group is opened or closed.
         *
         * @param syncTabGroupId The sync ID corresponding to the tab group.
         * @param localTabGroupId The new local ID of the tab group.
         */
        default void onTabGroupLocalIdChanged(
                String syncTabGroupId, @Nullable LocalTabGroupId localTabGroupId) {}
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
     * @param localTabGroupId The local tab group ID.
     * @return The sync ID of the group after it has been added to sync.
     */
    String createGroup(LocalTabGroupId localTabGroupId);

    /**
     * Removes a remote tab group which is open locally.
     *
     * @param localTabGroupId The local ID of the tab group removed.
     */
    void removeGroup(LocalTabGroupId localTabGroupId);

    /**
     * Removes a remote tab group which could be closed or open locally.
     *
     * @param syncTabGroupId The sync ID of the tab group removed.
     */
    void removeGroup(String syncTabGroupId);

    /**
     * Updates the visual info of the remote tab group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param title The title of the tab group.
     * @param color The color of the tab group.
     */
    void updateVisualData(
            LocalTabGroupId tabGroupId, @NonNull String title, @TabGroupColorId int color);

    /**
     * Makes the saved tab group a shared group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param collaborationId Collaboration ID with which the group is associated.
     */
    void makeTabGroupShared(LocalTabGroupId tabGroupId, @NonNull String collaborationId);

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
    void addTab(LocalTabGroupId tabGroupId, int tabId, String title, GURL url, int position);

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
    void updateTab(LocalTabGroupId tabGroupId, int tabId, String title, GURL url, int position);

    /**
     * Removes a tab from the remote tab group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param tabId The local tab ID of the tab being removed.
     */
    void removeTab(LocalTabGroupId tabGroupId, int tabId);

    /**
     * Moves a tab within a group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param tabId The local tab ID of the tab being removed.
     * @param newIndexInGroup The new index of the tab in the group.
     */
    void moveTab(LocalTabGroupId tabGroupId, int tabId, int newIndexInGroup);

    /**
     * Called to notify the backend that a tab was selected in the UI. Metrics purposes only.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param tabId The local ID of the corresponding tab.
     */
    void onTabSelected(LocalTabGroupId tabGroupId, int tabId);

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
    SavedTabGroup getGroup(LocalTabGroupId localGroupId);

    /**
     * Updates the in-memory mapping between sync and local tab group IDs.
     *
     * @param syncId The remote tab group ID.
     * @param localId The local tab group ID.
     */
    void updateLocalTabGroupMapping(
            String syncId, LocalTabGroupId localId, @OpeningSource int openingSource);

    /**
     * Removes the in-memory mapping between sync and local tab group IDs.
     *
     * @param localTabGroupId The local tab group ID whose mapping is to be forgotten.
     */
    void removeLocalTabGroupMapping(
            LocalTabGroupId localTabGroupId, @ClosingSource int closingSource);

    /**
     * Retrieves a list of group IDs that have been deleted from sync but haven't closed locally.
     * This can happen a lot in multi-window scenario where the deletion happened for a group that
     * belongs to a window that was closed when sync received this event.
     *
     * @return A list of {@link LocalTabGroupId} for groups that have been deleted.
     */
    List<LocalTabGroupId> getDeletedGroupIds();

    /**
     * Updates the in-memory mapping between sync and local IDs for a given tab.
     *
     * @param localGroupId The local group ID of the corresponding tab group.
     * @param syncTabId The sync ID of the corresponding tab.
     * @param localTabId The local ID of the corresponding tab.
     */
    void updateLocalTabId(LocalTabGroupId localGroupId, String syncTabId, int localTabId);

    /**
     * Helper method to identify whether a given sync cache guid corresponds to a remote device.
     *
     * @param syncCacheGuid A sync cache guid. Typically obtained from a tab group or tab
     *     attribution metadata.
     */
    boolean isRemoteDevice(String syncCacheGuid);

    /**
     * Called to explicitly record a tab group event. See native for full documentation.
     *
     * @param eventDetails The details about the event such as event type, source, and the
     *     associated tab group info.
     */
    void recordTabGroupEvent(EventDetails eventDetails);
}
