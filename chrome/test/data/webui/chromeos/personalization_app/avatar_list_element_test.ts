// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {AvatarCameraMode, AvatarListElement, UserActionName, UserImageObserver} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestUserProvider} from './test_user_interface_provider.js';

suite('AvatarListElementTest', function() {
  let avatarListElement: AvatarListElement|null;

  let testUserProvider: TestUserProvider;
  let testPersonalizationStore: TestPersonalizationStore;

  setup(function() {
    const mocks = baseSetup();
    testUserProvider = mocks.userProvider;
    testPersonalizationStore = mocks.personalizationStore;
    UserImageObserver.initUserImageObserverIfNeeded();
  });

  teardown(async () => {
    await teardownElement(avatarListElement);
    avatarListElement = null;
    UserImageObserver.shutdown();
  });

  test('fetches list of default avatar images and saves to store', async () => {
    avatarListElement = initElement(AvatarListElement);
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

  test('calls selectDefaultImage with correct index on click', async () => {
    testPersonalizationStore.data.user.defaultUserImages =
        testUserProvider.defaultUserImages;
    avatarListElement = initElement(AvatarListElement);

    const image =
        avatarListElement.shadowRoot!.querySelector(
            `div[data-id="${testUserProvider.defaultUserImages[0]!.index}"]`) as
        HTMLDivElement;

    image.click();
    const index = await testUserProvider.whenCalled('selectDefaultImage');
    assertEquals(testUserProvider.defaultUserImages[0]!.index, index);
  });

  test('fetches profile image and saves to store on load', async () => {
    testPersonalizationStore.setReducersEnabled(true);
    avatarListElement = initElement(AvatarListElement);

    await testUserProvider.whenCalled('setUserImageObserver');

    testPersonalizationStore.expectAction(UserActionName.SET_PROFILE_IMAGE);
    testUserProvider.userImageObserverRemote!.onUserProfileImageUpdated(
        testUserProvider.profileImage);

    const action = await testPersonalizationStore.waitForAction(
        UserActionName.SET_PROFILE_IMAGE);

    assertDeepEquals(
        {
          name: UserActionName.SET_PROFILE_IMAGE,
          profileImage: testUserProvider.profileImage,
        },
        action,
    );
    assertDeepEquals(
        testPersonalizationStore.data.user.profileImage,
        testUserProvider.profileImage);
  });

  test('calls selectProfileImage on click', async () => {
    testPersonalizationStore.data.user.profileImage =
        testUserProvider.profileImage;
    avatarListElement = initElement(AvatarListElement);

    const image = avatarListElement.shadowRoot!.getElementById(
                      'profileImage') as HTMLImageElement;

    image.click();
    await testUserProvider.whenCalled('selectProfileImage');
    assertDeepEquals(testUserProvider.profileImage, {
      url: 'data://updated_test_url',
    });
  });

  test('hides open camera button if no camera present', async () => {
    testPersonalizationStore.data.user.isCameraPresent = false;
    avatarListElement = initElement(AvatarListElement);
    await waitAfterNextRender(avatarListElement);

    assertEquals(
        null, avatarListElement!.shadowRoot!.getElementById('openCamera'),
        'open camera button does not exist');

    testPersonalizationStore.data.user.isCameraPresent = true;
    testPersonalizationStore.notifyObservers();
    await waitAfterNextRender(avatarListElement);

    assertTrue(
        !!avatarListElement!.shadowRoot!.getElementById('openCamera'),
        'open camera button exists');
  });

  test('click open camera button shows the avatar-camera modal', async () => {
    testPersonalizationStore.data.user.isCameraPresent = true;

    avatarListElement = initElement(AvatarListElement);
    await waitAfterNextRender(avatarListElement);

    assertTrue(
        !avatarListElement.shadowRoot!.querySelector('avatar-camera'),
        'avatar-camera element should not be open');

    const openCameraButton =
        avatarListElement!.shadowRoot!.getElementById('openCamera')!;
    openCameraButton.click();

    await waitAfterNextRender(avatarListElement);

    assertEquals(
        AvatarCameraMode.CAMERA,
        avatarListElement.shadowRoot?.querySelector('avatar-camera')?.mode,
        'avatar-camera should be visible and set to camera');
  });

  test('closes camera ui if camera goes offline', async () => {
    testPersonalizationStore.data.user.isCameraPresent = true;

    avatarListElement = initElement(AvatarListElement);
    await waitAfterNextRender(avatarListElement);

    avatarListElement.shadowRoot?.getElementById('openCamera')?.click();
    await waitAfterNextRender(avatarListElement);

    assertEquals(
        AvatarCameraMode.CAMERA,
        avatarListElement.shadowRoot?.querySelector('avatar-camera')?.mode,
        'avatar-camera should be set to camera');

    testPersonalizationStore.data.user.isCameraPresent = false;
    testPersonalizationStore.notifyObservers();

    await waitAfterNextRender(avatarListElement);

    assertTrue(
        !avatarListElement.shadowRoot!.querySelector('avatar-camera'),
        'avatar-camera should be gone because camera no longer available');
  });

  test('custom avatar selectors are shown with pref enabled', async () => {
    testPersonalizationStore.data.user.isCameraPresent = true;
    testPersonalizationStore.data.user.profileImage =
        testUserProvider.profileImage;
    avatarListElement = initElement(AvatarListElement);

    await waitAfterNextRender(avatarListElement);

    assertTrue(
        !!avatarListElement!.shadowRoot!.getElementById('openCamera'),
        'open camera button exists');
    assertTrue(
        !!avatarListElement!.shadowRoot!.getElementById('openVideo'),
        'open video button exists');
    assertTrue(
        !!avatarListElement!.shadowRoot!.getElementById('openFolder'),
        'open folder button exists');
    assertTrue(
        !!avatarListElement!.shadowRoot!.getElementById('profileImage'),
        'select profile image button exists');
  });

  test('custom avatar selectors are not shown with pref disabled', async () => {
    loadTimeData.overrideValues(
        {isUserAvatarCustomizationSelectorsEnabled: false});
    testPersonalizationStore.data.user.isCameraPresent = true;
    testPersonalizationStore.data.user.profileImage =
        testUserProvider.profileImage;
    avatarListElement = initElement(AvatarListElement);

    await waitAfterNextRender(avatarListElement);

    assertTrue(
        !avatarListElement!.shadowRoot!.getElementById('openCamera'),
        'open camera button does not exist');
    assertTrue(
        !avatarListElement!.shadowRoot!.getElementById('openVideo'),
        'open video button does not exist');
    assertTrue(
        !avatarListElement!.shadowRoot!.getElementById('openFolder'),
        'open folder button does not exist');
    assertTrue(
        !avatarListElement!.shadowRoot!.getElementById('profileImage'),
        'select profile button does not exist');
  });
});
