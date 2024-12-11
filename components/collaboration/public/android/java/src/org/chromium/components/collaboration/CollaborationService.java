// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.member_role.MemberRole;

/**
 * CollaborationService is the core class for managing collaboration group flows. It represents a
 * native CollaborationService object in Java.
 */
public interface CollaborationService {
    /**
     * Whether the service is an empty implementation. This is here because the Chromium build
     * disables RTTI, and we need to be able to verify that we are using an empty service from the
     * Chrome embedder.
     *
     * @return Whether the service implementation is empty.
     */
    @VisibleForTesting
    boolean isEmptyService();

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
}
