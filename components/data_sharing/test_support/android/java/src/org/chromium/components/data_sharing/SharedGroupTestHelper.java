// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.GaiaId;

/**
 * Test helpers for creating data_sharing objects and mocking calls to {@link DataSharingService}.
 */
public class SharedGroupTestHelper {
    public static final String COLLABORATION_ID1 = "collabId1";
    public static final String COLLABORATION_ID2 = "collabId2";
    public static final String ACCESS_TOKEN1 = "accessToken1";
    public static final GaiaId GAIA_ID1 = new GaiaId("gaiaId1");
    public static final GaiaId GAIA_ID2 = new GaiaId("gaiaId2");
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

    private final CollaborationService mCollaborationService;

    /**
     * @param mockCollaborationService A mock {@link CollaborationService}.
     */
    public SharedGroupTestHelper(CollaborationService mockCollaborationService) {
        mCollaborationService = mockCollaborationService;
    }

    /** Creates a new group member. */
    private static GroupMember newGroupMember(
            GaiaId gaiaId,
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
                collaborationId, /* displayName= */ null, members, /* accessToken= */ null);
    }

    /**
     * Sets up mocks to return {@link GroupData} to a getGroupData cal on {@link
     * CollaborationService}.
     */
    public void mockGetGroupData(String collaborationId, GroupMember... members) {
        when(mCollaborationService.getGroupData(eq(collaborationId)))
                .thenReturn(newGroupData(collaborationId, members));
    }

    /** Sets up mocks to return null to a getGroupData cal on {@link CollaborationService}. */
    public void mockGetGroupDataFailure(String collaborationId) {
        when(mCollaborationService.getGroupData(eq(collaborationId))).thenReturn(null);
    }
}
