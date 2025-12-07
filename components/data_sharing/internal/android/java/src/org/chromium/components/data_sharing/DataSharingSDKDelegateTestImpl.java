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
import org.chromium.components.sync.protocol.GroupMember;
import org.chromium.components.sync.protocol.MemberRole;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/** Implementation of {@link DataSharingSDKDelegate}. */
public class DataSharingSDKDelegateTestImpl implements DataSharingSDKDelegate {

    private final HashMap<String, List<GroupMember>> mMembers = new HashMap<>();

    /** Adds a list of members for a group. */
    public void addMembers(
            String groupId, org.chromium.components.data_sharing.GroupMember... members) {
        List<GroupMember> protoMembers = mMembers.getOrDefault(groupId, new ArrayList<>());
        for (var member : members) {
            protoMembers.add(convertToProtoMember(member));
        }
        mMembers.put(groupId, protoMembers);
    }

    private GroupMember convertToProtoMember(
            org.chromium.components.data_sharing.GroupMember member) {
        return GroupMember.newBuilder()
                .setGaiaId(member.gaiaId.toString())
                .setDisplayName(member.displayName)
                .setEmail(member.email)
                .setAvatarUrl(member.avatarUrl.getSpec())
                .setGivenName(member.givenName)
                .setRole(convertMemberRole(member.role))
                .build();
    }

    private MemberRole convertMemberRole(
            @org.chromium.components.data_sharing.member_role.MemberRole int role) {
        switch (role) {
            case org.chromium.components.data_sharing.member_role.MemberRole.UNKNOWN:
                return MemberRole.MEMBER_ROLE_UNSPECIFIED;
            case org.chromium.components.data_sharing.member_role.MemberRole.OWNER:
                return MemberRole.MEMBER_ROLE_OWNER;
            case org.chromium.components.data_sharing.member_role.MemberRole.MEMBER:
                return MemberRole.MEMBER_ROLE_MEMBER;
            case org.chromium.components.data_sharing.member_role.MemberRole.INVITEE:
                return MemberRole.MEMBER_ROLE_INVITEE;
            case org.chromium.components.data_sharing.member_role.MemberRole.FORMER_MEMBER:
                return MemberRole.MEMBER_ROLE_FORMER_MEMBER;
            default:
                return null;
        }
    }

    private void includeMembers(String groupId, GroupData.Builder builder) {
        for (GroupMember member : mMembers.getOrDefault(groupId, Collections.emptyList())) {
            builder.addMembers(member);
        }
    }

    @Override
    public void initialize(DataSharingNetworkLoader networkLoader) {}

    @Override
    public void createGroup(
            CreateGroupParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        String groupId = "test_group_id";
        GroupData.Builder groupData =
                GroupData.newBuilder().setGroupId(groupId).setDisplayName(params.getDisplayName());
        includeMembers(groupId, groupData);
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
            String groupId = params.getGroupParams(count - 1).getGroupId();

            GroupData.Builder groupData =
                    GroupData.newBuilder()
                            .setGroupId(groupId)
                            .setDisplayName("test_group_name_" + count);
            includeMembers(groupId, groupData);
            readGroupsResult.addGroupData(groupData.build());
        }
        callback.run(readGroupsResult.build().toByteArray(), /* status= */ 0);
    }

    @Override
    public void readGroupWithToken(
            ReadGroupWithTokenParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        ReadGroupsResult.Builder readGroupsResult = ReadGroupsResult.newBuilder();
        String groupId = params.getGroupId();
        GroupData.Builder groupData =
                GroupData.newBuilder().setGroupId(groupId).setDisplayName("test_group_name_0");
        includeMembers(groupId, groupData);
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
