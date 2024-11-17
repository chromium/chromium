// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    const groupData: GroupData = toMojomGroupData({
      groupId: groupId,
      displayName: groupName,
      accessToken,
      members: [{
        focusObfuscatedGaiaId: gaiaId,
        displayName,
        email,
        role: 'invitee',
        avatarUrl,
        givenName,
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
      }],
    };

    assertDeepEquals(expectedGroupData, groupData);
  });
});
