// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {Paths, UserImage, UserPreview} from 'chrome://personalization/trusted/personalization_app.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestUserProvider} from './test_user_interface_provider.js';

suite('UserPreviewTest', function() {
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

  test('displays edit icon when not managed', async () => {
    personalizationStore.data.user.image = userProvider.image;
    personalizationStore.data.user.imageIsEnterpriseManaged = false;
    userPreviewElement = initElement(UserPreview, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage =
        userPreviewElement!.shadowRoot!.getElementById('avatar');
    assertFalse(avatarImage!.classList.contains('managed'));
    // Does show edit icon.
    assertTrue(
        !!userPreviewElement!.shadowRoot!.getElementById('iconContainer'));
    // Does not show enterprise icon.
    assertFalse(!!userPreviewElement!.shadowRoot!.getElementById(
        'enterpriseIconContainer'));
  });

  test('displays user image from default image', async () => {
    personalizationStore.data.user.image = userProvider.image;
    userPreviewElement = initElement(UserPreview, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(
        userProvider.image.defaultImage?.url.url, avatarImage.src,
        'correct image url is shown for default image');
  });

  test('displays user image from profile image', async () => {
    personalizationStore.data.user.image = {profileImage: {}};
    personalizationStore.data.user.profileImage = userProvider.profileImage;
    userPreviewElement = initElement(UserPreview, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(
        userProvider.profileImage.url, avatarImage.src,
        'correct image url is shown for profile image');
  });

  test('displays user image from external image', async () => {
    // Use a cast here because generated types for |UserImage| are incorrect:
    // only one field should be specified at a time.
    const externalImage = {
      externalImage: {
        bytes: [0, 0, 0, 0],
        sharedMemory: undefined,
        invalidBuffer: false,
      },
    } as UserImage;
    personalizationStore.data.user.image = externalImage;

    userPreviewElement = initElement(UserPreview, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;

    assertTrue(
        avatarImage.src.startsWith('blob:'),
        'blob url is shown for external image');
  });

  test('displays non-clickable user image on user subpage', async () => {
    personalizationStore.data.user.image = userProvider.image;
    userPreviewElement = initElement(UserPreview, {path: Paths.USER});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar2') as HTMLImageElement;
    assertEquals(
        userProvider.image.defaultImage?.url.url, avatarImage.src,
        'default image url is shown on non-clickable image');
  });

  test('displays enterprise logo on avatar image', async () => {
    personalizationStore.data.user.image = userProvider.image;
    personalizationStore.data.user.imageIsEnterpriseManaged = true;
    userPreviewElement = initElement(UserPreview, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage =
        userPreviewElement!.shadowRoot!.getElementById('avatar');
    assertTrue(avatarImage!.classList.contains('managed'));
    // Does not show edit icon.
    assertFalse(
        !!userPreviewElement!.shadowRoot!.getElementById('iconContainer'));
    // Does show enterprise icon.
    assertTrue(!!userPreviewElement!.shadowRoot!.getElementById(
        'enterpriseIconContainer'));
  });
});
