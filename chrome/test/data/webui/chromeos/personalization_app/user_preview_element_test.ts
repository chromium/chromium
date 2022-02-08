// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserPreview} from 'chrome://personalization/trusted/user/user_preview_element.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestUserProvider} from './test_user_interface_provider.js';

export function UserPreviewTest() {
  let userPreviewElement: UserPreview|null;
  let personalizationStore: TestPersonalizationStore;
  let userProvider: TestUserProvider;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    userProvider = mocks.userProvider;
  });

  teardown(async () => {
    await teardownElement(userPreviewElement);
    userPreviewElement = null;
  });

  test('fetches user info on creation', async () => {
    assertEquals(0, userProvider.getCallCount('getUserInfo'));
    userPreviewElement = initElement(UserPreview);
    await userProvider.whenCalled('getUserInfo');
  });

  test('displays user info when set', async () => {
    personalizationStore.data.user.info = userProvider.info;
    userPreviewElement = initElement(UserPreview);
    await waitAfterNextRender(userPreviewElement!);

    assertEquals(
        userProvider.info.email,
        userPreviewElement!.shadowRoot!.getElementById('email')!.innerText);

    assertEquals(
        userProvider.info.name,
        userPreviewElement!.shadowRoot!.getElementById('name')!.innerText);
  });

  test('displays clickable user image on main page', async () => {
    personalizationStore.data.user.image = userProvider.image;
    userPreviewElement = initElement(UserPreview, {'clickable': true});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(userProvider.image.url, avatarImage.src);
  });

  test('displays non-clickable user image on user subpage', async () => {
    personalizationStore.data.user.image = userProvider.image;
    userPreviewElement = initElement(UserPreview);
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar2') as HTMLImageElement;
    assertEquals(userProvider.image.url, avatarImage.src);
  });
}
