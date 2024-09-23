// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {DefaultUserImage, Paths, UserImage, UserPreviewElement} from 'chrome://personalization/js/personalization_app.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestUserProvider} from './test_user_interface_provider.js';

suite('UserPreviewElementTest', function() {
  let userPreviewElement: UserPreviewElement|null;
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
    userPreviewElement = initElement(UserPreviewElement);
    await userProvider.whenCalled('getUserInfo');
  });

  test('displays user info when set', async () => {
    personalizationStore.data.user.info = userProvider.info;
    userPreviewElement = initElement(UserPreviewElement);
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
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
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
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(
        userProvider.image.defaultImage?.url!.url, avatarImage.src,
        'correct image url is shown for default image');
  });

  test('displays user image from profile image', async () => {
    personalizationStore.data.user.image = {profileImage: {}} as UserImage;
    personalizationStore.data.user.profileImage = userProvider.profileImage;
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(
        userProvider.profileImage.url, avatarImage.src,
        'correct image url is shown for profile image');
    assertTrue(
        avatarImage.src.startsWith('data:'), 'data url is not sanitized');
    assertTrue(
        !avatarImage.style.backgroundImage,
        'data url does not have a background image');
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

    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;

    assertTrue(
        avatarImage.src.startsWith('blob:'),
        'blob url is shown for external image');
    assertTrue(
        !avatarImage.style.backgroundImage,
        'blob url does not have a background image');
  });

  test('sanitizes gstatic image', async () => {
    personalizationStore.data.user.image = {
      'defaultImage': {
        url: {
          url: 'https://www.gstatic.com/',
        },
        title: stringToMojoString16('the remains of the day'),
        index: 1,
        sourceInfo: null,
      },
    };

    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;

    assertTrue(
        avatarImage.src.startsWith('chrome://image'), 'url was sanitized');
    assertTrue(
        avatarImage.style.backgroundImage.startsWith('url("chrome://image'),
        'background-url was sanitized');
  });

  test('do not display image if user image is not ready yet', async () => {
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(
        null, avatarImage,
        'do not display image if user image is not ready yet');
  });

  test('displays placeholder image if user image is invalid', async () => {
    personalizationStore.data.user.image = {invalidImage: {}} as UserImage;
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement!);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar') as HTMLImageElement;
    assertEquals(
        'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE', avatarImage.src,
        'placeholder image is shown for invalid user image');
  });

  test('displays non-clickable user image on user subpage', async () => {
    personalizationStore.data.user.image = userProvider.image;
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.USER});
    await waitAfterNextRender(userPreviewElement);

    const avatarImage = userPreviewElement!.shadowRoot!.getElementById(
                            'avatar2') as HTMLImageElement;
    assertEquals(
        userProvider.image.defaultImage?.url!.url, avatarImage.src,
        'default image url is shown on non-clickable image');
  });

  test('displays enterprise logo on avatar image', async () => {
    personalizationStore.data.user.image = userProvider.image;
    personalizationStore.data.user.imageIsEnterpriseManaged = true;
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
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

  test('displays author and website source info if present', async () => {
    personalizationStore.data.user.image = userProvider.image;
    userPreviewElement = initElement(UserPreviewElement, {path: Paths.ROOT});
    await waitAfterNextRender(userPreviewElement);

    // Image has no sourceInfo so should be missing.
    assertFalse(!!userPreviewElement.shadowRoot!.getElementById('sourceInfo'));

    const deprecatedDefaultImage: DefaultUserImage = {
      index: 2,
      title: stringToMojoString16('title'),
      url: {url: 'data://test_url'},
      sourceInfo: {
        author: stringToMojoString16('author example'),
        website: {url: 'website example'},
      },
    };
    personalizationStore.data.user.image = {
      defaultImage: deprecatedDefaultImage,
    } as UserImage;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(userPreviewElement);

    // |sourceInfo| element should now exist.
    const a = userPreviewElement.shadowRoot!.getElementById('sourceInfo');
    assertEquals('website example', a!.getAttribute('href'));
    assertEquals('author example', a!.querySelector('span')!.innerText);
  });
});
