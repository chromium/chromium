// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Information about a member of a group. */
@JNINamespace("data_sharing")
public class ServiceStatus {
    public @SigninStatus int signinStatus;
    public @SyncStatus int syncStatus;
    public @CollaborationStatus int collaborationStatus;

    /**
     * Constructor for a {@link ServiceStatus} object.
     *
     * @param signinStatus The signin status of the service.
     * @param syncStatus The sync status of the service.
     * @param collaborationStatus The collaboration status of the service.
     */
    public ServiceStatus(
            @SigninStatus int signinStatus,
            @SyncStatus int syncStatus,
            @CollaborationStatus int collaborationStatus) {
        this.signinStatus = signinStatus;
        this.syncStatus = syncStatus;
        this.collaborationStatus = collaborationStatus;
    }

    // Helper functions for checking DataSharingService's status.
    // Please keep the logic consistent with //components/data_sharing/public/service_status.h.

    /**
     * @return Whether the user is allowed to join a Data Sharing collaboration.
     */
    public boolean isAllowedToJoin() {
        switch (collaborationStatus) {
            case CollaborationStatus.DISABLED:
            case CollaborationStatus.DISABLED_FOR_POLICY:
                return false;
            case CollaborationStatus.ALLOWED_TO_JOIN:
            case CollaborationStatus.ENABLED_JOIN_ONLY:
            case CollaborationStatus.ENABLED_CREATE_AND_JOIN:
                return true;
        }
        return false;
    }

    /**
     * @return Whether the user is allowed to create a Data Sharing collaboration.
     */
    public boolean isAllowedToCreate() {
        switch (collaborationStatus) {
            case CollaborationStatus.DISABLED:
            case CollaborationStatus.DISABLED_FOR_POLICY:
            case CollaborationStatus.ALLOWED_TO_JOIN:
            case CollaborationStatus.ENABLED_JOIN_ONLY:
                return false;
            case CollaborationStatus.ENABLED_CREATE_AND_JOIN:
                return true;
        }
        return false;
    }

    @CalledByNative
    private static ServiceStatus createServiceStatus(
            int signinStatus, int syncStatus, int collaborationStatus) {
        return new ServiceStatus(signinStatus, syncStatus, collaborationStatus);
    }
}
