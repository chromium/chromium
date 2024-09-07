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

    const groupData: GroupData = toMojomGroupData({
      id: groupId,
      name: groupName,
      members: [{
        profileId: gaiaId,
        displayName: displayName,
        displayValue: email,
        role: 'invitee',
        photoUrl: avatarUrl,
      }],
    });

    const expectedGroupData: GroupData = {
      groupId: groupId,
      displayName: groupName,
      accessToken: '',
      members: [{
        gaiaId: gaiaId,
        displayName: displayName,
        email: email,
        role: MemberRole.kInvitee,
        avatarUrl: {url: avatarUrl},
      }],
    };

    assertDeepEquals(expectedGroupData, groupData);
  });
});
