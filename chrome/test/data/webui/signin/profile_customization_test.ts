// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-customization/profile_customization_app.js';

import type {ProfileCustomizationAppElement} from 'chrome://profile-customization/profile_customization_app.js';
import {ProfileCustomizationBrowserProxyImpl} from 'chrome://profile-customization/profile_customization_browser_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProfileCustomizationBrowserProxy} from './test_profile_customization_browser_proxy.js';

suite('ProfileCustomizationTest', function() {
  let app: ProfileCustomizationAppElement;
  let browserProxy: TestProfileCustomizationBrowserProxy;

  const AVATAR_URL_1 = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  const AVATAR_URL_2 = 'chrome://theme/IDR_PROFILE_AVATAR_2';
  const STATIC_TITLE_PATTERN = /^Customize your (Chromium|Chrome) profile$/g;

  setup(function() {
    loadTimeData.overrideValues({
      profileName: 'TestName',
      isLocalProfileCreation: false,
    });
    browserProxy = new TestProfileCustomizationBrowserProxy();
    browserProxy.setProfileInfo({
      backgroundColor: 'rgb(0, 255, 0)',
      pictureUrl: AVATAR_URL_1,
      isManaged: false,
      welcomeTitle: '',
    });
    ProfileCustomizationBrowserProxyImpl.setInstance(browserProxy);
  });

  async function initializeApp() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('profile-customization-app');
    document.body.append(app);
    await browserProxy.whenCalled('initialized');
  }

  function checkImageUrl(elementId: string, expectedUrl: string) {
    assertTrue(isChildVisible(app, elementId));
    const img = app.shadowRoot!.querySelector<HTMLImageElement>(elementId)!;
    assertEquals(expectedUrl, img.src);
  }

  // Checks that clicking Done without interacting with the input does not
  // change the name.
  test('ClickDone', async function() {
    await initializeApp();
    // Wait for the name input to update, and then wait a second time
    // for the validation that is triggered before checking if the
    // button is disabled.
    await app.$.nameInput.updateComplete;
    await app.$.nameInput.updateComplete;
    assertTrue(isChildVisible(app, '#doneButton'));
    const doneButton = app.$.doneButton;
    assertFalse(doneButton.disabled);
    doneButton.click();
    const profileName = await browserProxy.whenCalled('done');
    assertEquals('TestName', profileName);
  });

  // Checks that the name can be changed.
  test('ChangeName', async function() {
    await initializeApp();
    const nameInput = app.$.nameInput;
    await nameInput.updateComplete;
    // Check the default value for the input.
    assertEquals('TestName', nameInput.value);
    assertFalse(nameInput.invalid);

    // Invalid name (white space).
    nameInput.value = '   ';
    // Wait for the name input to update, and then wait a second time
    // for the validation that is triggered before checking if the
    // button is disabled.
    await nameInput.updateComplete;
    assertTrue(nameInput.invalid);

    // The button is disabled.
    await nameInput.updateComplete;
    assertTrue(isChildVisible(app, '#doneButton'));
    const doneButton = app.$.doneButton;
    assertTrue(doneButton.disabled);

    // Empty name.
    nameInput.value = '';
    await nameInput.updateComplete;
    assertTrue(nameInput.invalid);
    await nameInput.updateComplete;
    assertTrue(doneButton.disabled);

    // Valid name.
    nameInput.value = 'Bob';
    await nameInput.updateComplete;
    assertFalse(nameInput.invalid);
    await nameInput.updateComplete;

    // Click done, and check that the new name is sent.
    assertTrue(isChildVisible(app, '#doneButton'));
    assertFalse(doneButton.disabled);
    doneButton.click();
    const profileName = await browserProxy.whenCalled('done');
    assertEquals('Bob', profileName);
  });

  test('ProfileInfo', async function() {
    await initializeApp();
    // Check initial info.
    assertNotEquals(null, app.$.title.innerText.match(STATIC_TITLE_PATTERN));
    checkImageUrl('#avatar', AVATAR_URL_1);
    assertFalse(isChildVisible(app, '#workBadge'));
    assertFalse(isChildVisible(app, '#customizeAvatarIcon'));
    // Update the info.
    const color2 = 'rgb(4, 5, 6)';
    webUIListenerCallback('on-profile-info-changed', {
      backgroundColor: color2,
      pictureUrl: AVATAR_URL_2,
      isManaged: true,
      welcomeTitle: '',
    });
    await microtasksFinished();
    assertNotEquals(null, app.$.title.innerText.match(STATIC_TITLE_PATTERN));
    checkImageUrl('#avatar', AVATAR_URL_2);
    assertTrue(isChildVisible(app, '#workBadge'));
    assertFalse(isChildVisible(app, '#customizeAvatarIcon'));
  });

  test('ThemeColorPicker', async function() {
    await initializeApp();
    assertTrue(!!app.shadowRoot!.querySelector('cr-theme-color-picker'));
  });

  // Checks that there is no Delete Profile button in the default Profile
  // Customization page.
  test('DeleteProfileButtonNotVisible', async function() {
    await initializeApp();
    assertFalse(isChildVisible(app, '#deleteProfileButton'));
  });

  // Checks that clicking the Skip button triggers the correct browser proxy
  // method.
  test('ClickSkip', async function() {
    await initializeApp();
    assertTrue(isChildVisible(app, '#skipButton'));
    const skipButton =
        app.shadowRoot!.querySelector<CrButtonElement>('#skipButton')!;
    skipButton.click();
    return browserProxy.whenCalled('skip');
  });
});

suite(`LocalProfileCreationTest`, function() {
  let app: ProfileCustomizationAppElement;
  let browserProxy: TestProfileCustomizationBrowserProxy;

  const AVATAR_URL_1 = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  const WELCOME_TITLE = 'Welcome!';

  setup(async function() {
    loadTimeData.overrideValues({
      profileName: 'TestName',
      isLocalProfileCreation: true,
    });
    browserProxy = new TestProfileCustomizationBrowserProxy();
    browserProxy.setProfileInfo({
      backgroundColor: 'rgb(0, 255, 0)',
      pictureUrl: AVATAR_URL_1,
      isManaged: false,
      welcomeTitle: '',
    });
    ProfileCustomizationBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('profile-customization-app');
    document.body.append(app);
    await browserProxy.whenCalled('initialized');
    return microtasksFinished();
  });

  test('LocalProfileCreationDialog', function() {
    assertEquals(WELCOME_TITLE, app.$.title.innerText);
    assertFalse(isChildVisible(app, '#workBadge'));
    assertTrue(isChildVisible(app, '#customizeAvatarIcon'));
    assertTrue(isChildVisible(app, '#deleteProfileButton'));
    assertFalse(isChildVisible(app, '#skipButton'));

    const activeView = 'active';
    const profileCustomizationDialog =
        app.shadowRoot!.querySelector<HTMLElement>('#customizeDialog')!;
    const avatarSelectionDialog =
        app.shadowRoot!.querySelector<HTMLElement>('#selectAvatarDialog')!;
    assertTrue(profileCustomizationDialog.classList.contains(activeView));
    assertFalse(avatarSelectionDialog.classList.contains(activeView));

    // Open avatar customization.
    const avatarCustomizationButton =
        app.shadowRoot!.querySelector<CrIconButtonElement>(
            '#customizeAvatarIcon')!;
    avatarCustomizationButton.click();
    assertFalse(profileCustomizationDialog.classList.contains(activeView));
    assertTrue(avatarSelectionDialog.classList.contains(activeView));

    const selectAvatarConfirmButton =
        app.shadowRoot!.querySelector<CrButtonElement>(
            '#selectAvatarConfirmButton')!;
    selectAvatarConfirmButton.click();
    assertTrue(profileCustomizationDialog.classList.contains(activeView));
    assertFalse(avatarSelectionDialog.classList.contains(activeView));
  });

  test('ClickDeleteProfileButton', function() {
    assertTrue(isChildVisible(app, '#deleteProfileButton'));
    const deleteProfileButton =
        app.shadowRoot!.querySelector<CrButtonElement>('#deleteProfileButton')!;
    deleteProfileButton.click();
    return browserProxy.whenCalled('deleteProfile');
  });
});
