// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

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
         * A new tab group was added, or an existing tab group was updated at the given source.
         *
         * @param group The {@link SavedTabGroup} that was added or updated.
         * @param source The source of the change (local or remote).
         */
        void onTabGroupAddedOrUpdated(SavedTabGroup group, @TriggerSource int source);

        /**
         * Called when a tab group is deleted from sync. The local tab group should be deleted in
         * response and all the corresponding tabs should be closed. TODO(b/331466817): Build
         * mechanism to distinguish between tab group deletion and ungroup events correctly.
         *
         * @param localId The local ID corresponding to the tab group that was removed.
         */
        void onTabGroupRemoved(int localId);
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
     * Removes a group from sync in response to a local group removal.
     *
     * @param groupId The ID of the tab group removed. Currently this is the root ID of the group.
     */
    void removeGroup(int groupId);
}
