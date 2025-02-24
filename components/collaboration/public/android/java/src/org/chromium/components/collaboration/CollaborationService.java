// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.url.GURL;

/**
 * CollaborationService is the core class for managing collaboration group flows. It represents a
 * native CollaborationService object in Java.
 */
public interface CollaborationService {
    /** Observers for listening updates from the CollaborationService. */
    interface Observer {
        /**
         * Called when the service status has changed.
         *
         * @param oldStatus The previous service status.
         * @param newStatus The current service status.
         */
        default void onServiceStatusChanged(ServiceStatus oldStatus, ServiceStatus newStatus) {}
    }

    /**
     * Whether the service is an empty implementation. This is here because the Chromium build
     * disables RTTI, and we need to be able to verify that we are using an empty service from the
     * Chrome embedder.
     *
     * @return Whether the service implementation is empty.
     */
    @VisibleForTesting
    boolean isEmptyService();

    /**
     * Starts a new collaboration join flow.
     *
     * @param delegate The delegate to perform action on the Android UI.
     * @param url The URL of the join request.
     */
    void startJoinFlow(CollaborationControllerDelegate delegate, GURL url);

    /**
     * Starts a new collaboration share or manage flow.
     *
     * @param delegate The delegate to perform action on the Android UI.
     * @param either_id The ID to identify a tab group.
     */
    void startShareOrManageFlow(CollaborationControllerDelegate delegate, String syncId);

    /** Returns the current {@link ServiceStatus} of the service. */
    @NonNull
    ServiceStatus getServiceStatus();

    /**
     * Get the member role of the current primary user for a collaboration group.
     *
     * @param collaborationId The collaboration group id.
     * @return The {@link MemberRole} of the current user. UNKNOWN is returned if no user or group
     *     found.
     */
    @MemberRole
    int getCurrentUserRoleForGroup(String collaborationId);

    /**
     * Synchronously get group data for a given group id.
     *
     * @param collaborationId The collaboration group id.
     * @return The {@link GroupData} of the group.
     */
    @Nullable
    GroupData getGroupData(String collaborationId);

    /**
     * Attempt to leave a collaboration group.
     *
     * @param groupId The group ID to leave.
     * @param callback The leave result as a boolean.
     */
    void leaveGroup(String groupId, Callback</* success= */ Boolean> callback);

    /**
     * Attempt to delete a collaboration group.
     *
     * @param groupId The group ID to delete.
     * @param callback The deletion result as a boolean.
     */
    void deleteGroup(String groupId, Callback</* success= */ Boolean> callback);

    /**
     * Add an observer to be notified of the backend changes.
     *
     * @param observer The observer to be notified.
     */
    void addObserver(CollaborationService.Observer observer);

    /**
     * Remove a given observer.
     *
     * @param observer The observer to be removed.
     */
    void removeObserver(CollaborationService.Observer observer);
}
