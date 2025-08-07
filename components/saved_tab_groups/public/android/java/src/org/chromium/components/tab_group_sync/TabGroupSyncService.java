// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.List;

/**
 * The core service class for handling tab group sync across devices. Provides 1. Mutation methods
 * to propagate local changes to remote. 2. Observer interface to propagate remote changes to local.
 */
@NullMarked
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

        /**
         * Called to notify that the local observation mode has changed. If {@code
         * observeLocalChanges} is true, the local changes should be ignored by the observer and not
         * propagated to sync. This is typically used to ignore transient changes which are often
         * redundant and incorrect to propagate to sync server. Note, this is a temporary solution
         * and only available in Android until a more accurate cross-platform solution is
         * implemented.
         *
         * @param observeLocalChanges Whether the local tab model changes should be observed.
         */
        default void onLocalObservationModeChanged(boolean observeLocalChanges) {}
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
     * Adds a given {@link SavedTabGroup} to the service.
     *
     * @param savedTabGroup The {@link SavedTabGroup} to be added to the service.
     */
    void addGroup(SavedTabGroup savedTabGroup);

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
    void updateVisualData(LocalTabGroupId tabGroupId, String title, @TabGroupColorId int color);

    /**
     * Makes the saved tab group a shared group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param collaborationId Collaboration ID with which the group is associated.
     * @param callback Callback to be called when group is converted to shared tab group.
     */
    void makeTabGroupShared(
            LocalTabGroupId tabGroupId,
            String collaborationId,
            @Nullable Callback<Boolean> tabGroupSharingCallback);

    /**
     * Starts the process of converting a shared tab group to saved tab group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param callback Callback to be called when group is converted to saved tab group.
     */
    void aboutToUnShareTabGroup(LocalTabGroupId tabGroupId, @Nullable Callback<Boolean> callback);

    /**
     * Called when shared tab group is successfully converted to saved tab group.
     *
     * @param tabGroupId The local group ID of the corresponding tab group.
     * @param success boolean for telling if the operation succeeded or failed.
     */
    void onTabGroupUnShareComplete(LocalTabGroupId tabGroupId, boolean success);

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
     * Called to notify the backend that a tab was selected in the UI. Used by the messaging backend
     * to keep track of currently selected tab.
     *
     * @param tabGroupId The local group ID of the corresponding tab group. Null if the tab is not
     *     part of a group.
     * @param tabId The local ID of the corresponding tab.
     * @param tabTitle The title of the corresponding tab.
     */
    void onTabSelected(@Nullable LocalTabGroupId tabGroupId, int tabId, String tabTitle);

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
    @Nullable
    SavedTabGroup getGroup(String syncGroupId);

    /**
     * Returns a single {@link SavedTabGroup}.
     *
     * @param localGroupId The local ID of the group to be returned.
     * @return The associated {@link SavedTabGroup}.
     */
    @Nullable SavedTabGroup getGroup(LocalTabGroupId localGroupId);

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
     * Whether the TabGroupSyncService should observe local tab model changes. By default,
     * TabGroupSyncService starts observing from the start. Typically called to pause / resume local
     * observation typically during transient UI actions (such as dragging tab groups to other
     * windows). This ensures that transient changes which are often redundant and incorrect do not
     * propagate to sync server.
     *
     * @param observeLocalChanges True to observe local changes, false to pause observation.
     */
    void setLocalObservationMode(boolean observeLocalChanges);

    /** Returns whether TabGroupSyncService is currently observing local changes. */
    boolean isObservingLocalChanges();

    /**
     * Helper method to identify whether a given sync cache guid corresponds to a remote device.
     *
     * @param syncCacheGuid A sync cache guid. Typically obtained from a tab group or tab
     *     attribution metadata.
     */
    boolean isRemoteDevice(@Nullable String syncCacheGuid);

    /**
     * Returns whether a tab group with the given `sync_tab_group_id` was previously closed on this
     * device. Reset to false whenever the user opens the group intentionally.
     *
     * @param syncTabGroupId The sync ID of the associated tab group.
     */
    boolean wasTabGroupClosedLocally(String syncTabGroupId);

    /**
     * Called to explicitly record a tab group event. See native for full documentation.
     *
     * @param eventDetails The details about the event such as event type, source, and the
     *     associated tab group info.
     */
    void recordTabGroupEvent(EventDetails eventDetails);

    /**
     * Update the archival status of the local tab group.
     *
     * @param syncTabGroupId The sync ID of the tab group to be updated.
     * @param archivalStatus Whether the tab group should be archived locally or not.
     */
    void updateArchivalStatus(String syncTabGroupId, boolean archivalStatus);

    /**
     * For testing only. This is needed to test shared tab groups flow without depending on real
     * people groups from data sharing service backend.
     *
     * @param collaborationId Collaboration ID with which the collaboration group is associated.
     */
    void setCollaborationAvailableInFinderForTesting(String collaborationId);

    /**
     * @return The {@link VersioningMessageController} which is responsible for business logic
     *     related to shared tab groups versioning related messages.
     */
    @Nullable VersioningMessageController getVersioningMessageController();
}
