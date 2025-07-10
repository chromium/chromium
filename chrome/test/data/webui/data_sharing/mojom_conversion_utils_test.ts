// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataSharingMemberRoleEnum} from 'chrome-untrusted://data-sharing/data_sharing_sdk_types.js';
import {MemberRole} from 'chrome-untrusted://data-sharing/group_data.mojom-webui.js';
import type {GroupData} from 'chrome-untrusted://data-sharing/group_data.mojom-webui.js';
import {toMojomGroupData} from 'chrome-untrusted://data-sharing/mojom_conversion_utils.js';
import {assertDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('MojomConversionUtilsTest', () => {
  test('toMojomGroupData', () => {
    const groupId: string = 'TEST_GROUP_ID';
    const groupName: string = 'TEST_GROUP_NAME';
    const avatarUrl: string = 'http://example.com';
    const displayName: string = 'TEST_DISPLAY_NAME';
    const gaiaId: string = 'TEST_GAIA_ID';
    const email: string = 'test@gmail.com';
    const accessToken: string = 'testAccessToken';
    const givenName: string = 'TEST_GIVEN_NAME';

    const formerAvatarUrl: string = 'http://former.com';
    const formerDisplayName: string = 'FORMER_DISPLAY_NAME';
    const formerGaiaId: string = 'FORMER_GAIA_ID';
    const formerEmail: string = 'former@gmail.com';
    const formerGivenName: string = 'TEST_FORMER_GIVEN_NAME';


    const groupData: GroupData = toMojomGroupData({
      groupId: groupId,
      displayName: groupName,
      accessToken,
      members: [{
        focusObfuscatedGaiaId: gaiaId,
        displayName,
        email,
        role: DataSharingMemberRoleEnum.INVITEE,
        avatarUrl,
        givenName,
        createdAtTimeMs: 300,
        lastUpdatedAtTimeMs: 400,
      }],
      formerMembers: [{
        focusObfuscatedGaiaId: formerGaiaId,
        displayName: formerDisplayName,
        email: formerEmail,
        role: DataSharingMemberRoleEnum.FORMER_MEMBER,
        avatarUrl: formerAvatarUrl,
        givenName: formerGivenName,
        createdAtTimeMs: 100,
        lastUpdatedAtTimeMs: 200,
      }],
    });

    const expectedGroupData: GroupData = {
      groupId,
      displayName: groupName,
      accessToken,
      members: [{
        gaiaId,
        displayName,
        email,
        role: MemberRole.kInvitee,
        avatarUrl: {url: avatarUrl},
        givenName,
        creationTime: new Date(300),
        lastUpdatedTime: new Date(400),
      }],
      formerMembers: [{
        gaiaId: formerGaiaId,
        displayName: formerDisplayName,
        email: formerEmail,
        role: MemberRole.kFormerMember,
        avatarUrl: {url: formerAvatarUrl},
        givenName: formerGivenName,
        creationTime: new Date(100),
        lastUpdatedTime: new Date(200),
      }],
    };

    assertDeepEquals(expectedGroupData, groupData);
  });
});
