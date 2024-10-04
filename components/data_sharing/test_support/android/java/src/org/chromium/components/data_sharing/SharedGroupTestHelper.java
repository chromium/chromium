// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import org.mockito.ArgumentCaptor;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.member_role.MemberRole;

/**
 * Test helpers for creating data_sharing objects and mocking calls to {@link DataSharingService}.
 */
public class SharedGroupTestHelper {
    public static final String GAIA_ID1 = "gaiaId1";
    public static final String GAIA_ID2 = "gaiaId2";
    public static final String DISPLAY_NAME1 = "Jane Doe";
    public static final String DISPLAY_NAME2 = "John Doe";
    public static final String EMAIL1 = "one@gmail.com";
    public static final String EMAIL2 = "two@gmail.com";
    public static final String GIVEN_NAME1 = "Jane";
    public static final String GIVEN_NAME2 = "John";
    public static final GroupMember GROUP_MEMBER1 =
            newGroupMember(GAIA_ID1, DISPLAY_NAME1, EMAIL1, MemberRole.OWNER, GIVEN_NAME1);
    public static final GroupMember GROUP_MEMBER2 =
            newGroupMember(GAIA_ID2, DISPLAY_NAME2, EMAIL2, MemberRole.MEMBER, GIVEN_NAME2);

    private final DataSharingService mDataSharingService;
    private final ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;

    /**
     * @param mockDataSharingService A mock {@link DataSharingService}.
     * @param readGroupCallbackCaptor Will capture the read group callback.
     */
    public SharedGroupTestHelper(
            DataSharingService mockDataSharingService,
            ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> readGroupCallbackCaptor) {
        mDataSharingService = mockDataSharingService;
        mReadGroupCallbackCaptor = readGroupCallbackCaptor;
    }

    /** Creates a new group member. */
    private static GroupMember newGroupMember(
            String gaiaId,
            String displayName,
            String email,
            @MemberRole int memberRole,
            String givenName) {
        return new GroupMember(
                gaiaId, displayName, email, memberRole, /* avatarUrl= */ null, givenName);
    }

    /** Creates new group data. */
    public static GroupData newGroupData(String collaborationId, GroupMember... members) {
        return new GroupData(
                collaborationId, /* displayName= */ null, members, /* groupToken= */ null);
    }

    /** Responds to a readGroup call on the {@link DataSharingService}. */
    public void respondToReadGroup(String collaborationId, GroupMember... members) {
        verify(mDataSharingService, atLeastOnce())
                .readGroup(eq(collaborationId), mReadGroupCallbackCaptor.capture());
        GroupData groupData = newGroupData(collaborationId, members);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);
    }
}
