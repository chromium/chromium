// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.protocol.AddAccessTokenParams;
import org.chromium.components.data_sharing.protocol.AddAccessTokenResult;
import org.chromium.components.data_sharing.protocol.AddMemberParams;
import org.chromium.components.data_sharing.protocol.CreateGroupParams;
import org.chromium.components.data_sharing.protocol.CreateGroupResult;
import org.chromium.components.data_sharing.protocol.DeleteGroupParams;
import org.chromium.components.data_sharing.protocol.LeaveGroupParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailResult;
import org.chromium.components.data_sharing.protocol.ReadGroupWithTokenParams;
import org.chromium.components.data_sharing.protocol.ReadGroupsParams;
import org.chromium.components.data_sharing.protocol.ReadGroupsResult;
import org.chromium.components.data_sharing.protocol.RemoveMemberParams;
import org.chromium.components.sync.protocol.GroupData;

/** Implementation of {@link DataSharingSDKDelegate}. */
public class DataSharingSDKDelegateTestImpl implements DataSharingSDKDelegate {

    @Override
    public void initialize(DataSharingNetworkLoader networkLoader) {}

    @Override
    public void createGroup(
            CreateGroupParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        GroupData.Builder groupData =
                GroupData.newBuilder()
                        .setGroupId("test_group_id")
                        .setDisplayName(params.getDisplayName());
        CreateGroupResult.Builder createGroupResult =
                CreateGroupResult.newBuilder().setGroupData(groupData.build());
        callback.run(createGroupResult.build().toByteArray(), /* status= */ 0);
    }

    @Override
    public void readGroups(
            ReadGroupsParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        int groupsCount = params.getGroupParamsCount();
        ReadGroupsResult.Builder readGroupsResult = ReadGroupsResult.newBuilder();
        for (int count = 1; count <= groupsCount; count++) {
            GroupData.Builder groupData =
                    GroupData.newBuilder()
                            .setGroupId(params.getGroupParams(count - 1).getGroupId())
                            .setDisplayName("test_group_name_" + count);
            readGroupsResult.addGroupData(groupData.build());
        }
        callback.run(readGroupsResult.build().toByteArray(), /* status= */ 0);
    }

    @Override
    public void readGroupWithToken(
            ReadGroupWithTokenParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        ReadGroupsResult.Builder readGroupsResult = ReadGroupsResult.newBuilder();
        GroupData.Builder groupData =
                GroupData.newBuilder()
                        .setGroupId(params.getGroupId())
                        .setDisplayName("test_group_name_0");
        readGroupsResult.addGroupData(groupData.build());
        callback.run(readGroupsResult.build().toByteArray(), /* status= */ 0);
    }

    @Override
    public void addMember(AddMemberParams params, Callback<Integer> callback) {
        callback.onResult(/* result= */ 0);
    }

    @Override
    public void removeMember(RemoveMemberParams params, Callback<Integer> callback) {
        callback.onResult(/* result= */ 1);
    }

    @Override
    public void leaveGroup(LeaveGroupParams params, Callback<Integer> callback) {
        callback.onResult(/* result= */ 0);
    }

    @Override
    public void deleteGroup(DeleteGroupParams params, Callback<Integer> callback) {
        callback.onResult(/* result= */ 0);
    }

    @Override
    public void lookupGaiaIdByEmail(
            LookupGaiaIdByEmailParams params,
            DataSharingSDKDelegateProtoResponseCallback callback) {
        LookupGaiaIdByEmailResult.Builder lookupGaiaIdByEmailResult =
                LookupGaiaIdByEmailResult.newBuilder().setGaiaId(params.getEmail());
        callback.run(lookupGaiaIdByEmailResult.build().toByteArray(), /* status= */ 0);
    }

    @Override
    public void addAccessToken(
            AddAccessTokenParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        GroupData.Builder groupData = GroupData.newBuilder().setGroupId("test_group_id");
        AddAccessTokenResult.Builder addTokenResult =
                AddAccessTokenResult.newBuilder().setGroupData(groupData.build());
        callback.run(addTokenResult.build().toByteArray(), /* status= */ 0);
    }
}
