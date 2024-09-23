// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.protocol.AddAccessTokenParams;
import org.chromium.components.data_sharing.protocol.AddMemberParams;
import org.chromium.components.data_sharing.protocol.CreateGroupParams;
import org.chromium.components.data_sharing.protocol.DeleteGroupParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailParams;
import org.chromium.components.data_sharing.protocol.ReadGroupsParams;
import org.chromium.components.data_sharing.protocol.RemoveMemberParams;

/**
 * Copy of C++ DataSharingSDKDelegate in Java. Used by DataSharingService to provide access to SDK.
 */
public interface DataSharingSDKDelegate {

    void initialize(DataSharingNetworkLoader networkLoader);

    void createGroup(
            CreateGroupParams params, DataSharingSDKDelegateProtoResponseCallback callback);

    void readGroups(ReadGroupsParams params, DataSharingSDKDelegateProtoResponseCallback callback);

    /** Callback return DataSharingSDKDelegateProtoResponseCallback.Status as integer. */
    void addMember(AddMemberParams params, Callback<Integer> callback);

    /** Callback return DataSharingSDKDelegateProtoResponseCallback.Status as integer. */
    void removeMember(RemoveMemberParams params, Callback<Integer> callback);

    /** Callback return DataSharingSDKDelegateProtoResponseCallback.Status as integer. */
    void deleteGroup(DeleteGroupParams params, Callback<Integer> callback);

    void lookupGaiaIdByEmail(
            LookupGaiaIdByEmailParams params, DataSharingSDKDelegateProtoResponseCallback callback);

    void addAccessToken(
            AddAccessTokenParams params, DataSharingSDKDelegateProtoResponseCallback callback);
}
