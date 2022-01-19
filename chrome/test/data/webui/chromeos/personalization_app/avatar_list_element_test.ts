// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AvatarList} from 'chrome://personalization/trusted/user/avatar_list_element.js';
import {UserActionName} from 'chrome://personalization/trusted/user/user_actions.js';
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestUserProvider} from './test_user_interface_provider.js';

export function AvatarListTest() {
  let avatarListElement: AvatarList|null;

  let testUserProvider: TestUserProvider;
  let testPersonalizationStore: TestPersonalizationStore;

  setup(function() {
    const mocks = baseSetup();
    testUserProvider = mocks.userProvider;
    testPersonalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(avatarListElement);
    avatarListElement = null;
  });

  test('fetches list of default avatar images and saves to store', async () => {
    avatarListElement = initElement(AvatarList);
    testPersonalizationStore.expectAction(
        UserActionName.SET_DEFAULT_USER_IMAGES);
    await testUserProvider.whenCalled('getDefaultUserImages');
    const action = await testPersonalizationStore.waitForAction(
        UserActionName.SET_DEFAULT_USER_IMAGES);

    assertDeepEquals(
        {
          name: UserActionName.SET_DEFAULT_USER_IMAGES,
          defaultUserImages: testUserProvider.defaultUserImages,
        },
        action,
    );
  });
}
