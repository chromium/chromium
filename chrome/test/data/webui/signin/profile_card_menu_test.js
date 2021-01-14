// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ManageProfilesBrowserProxyImpl, ProfileState} from 'chrome://profile-picker/profile_picker.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfileCardMenuTest', function() {
  /** @type {!ProfileCardMenuElement} */
  let profileCardMenuElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  const menuButtonIndex = {
    CUSTOMIZE: 0,
    DELETE: 1,
  };

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    profileCardMenuElement = /** @type {!ProfileCardMenuElement} */ (
        document.createElement('profile-card-menu'));
    document.body.appendChild(profileCardMenuElement);
    const testProfileState = /** @type {!ProfileState} */ ({
      profilePath: `profilePath`,
      localProfileName: `profile`,
      isSyncing: true,
      needsSignin: false,
      gaiaName: `User`,
      userName: `User@gmail.com`,
      isManaged: false,
      avatarIcon: `AvatarUrl`,
    });
    profileCardMenuElement.profileState = testProfileState;
    return waitBeforeNextRender(profileCardMenuElement);
  });

  // Checks basic layout of the action menu.
  test('ProfileCardMenuActionMenu', async function() {
    assertFalse(profileCardMenuElement.$$('#actionMenu').open);
    assertFalse(profileCardMenuElement.$$('#removeConfirmationDialog').open);
    profileCardMenuElement.$$('#moreActionsButton').click();
    assertTrue(profileCardMenuElement.$$('#actionMenu').open);
    assertFalse(profileCardMenuElement.$$('#removeConfirmationDialog').open);
    const menuButtons = profileCardMenuElement.shadowRoot.querySelectorAll(
        '#actionMenu > .dropdown-item');
    assertEquals(menuButtons.length, 2);
  });

  // Click on the customize profile menu item calls native to open the profile
  // settings page.
  test('ProfileCardMenuCustomizeButton', async function() {
    profileCardMenuElement.$$('#moreActionsButton').click();
    const menuButtons = profileCardMenuElement.$$('#actionMenu')
                            .querySelectorAll('.dropdown-item');
    menuButtons[menuButtonIndex.CUSTOMIZE].click();
    await browserProxy.whenCalled('openManageProfileSettingsSubPage');
    assertFalse(profileCardMenuElement.$$('#actionMenu').open);
    assertFalse(profileCardMenuElement.$$('#removeConfirmationDialog').open);
  });

  // Click on the delete profile menu item opens the remove confirmation dialog.
  test('ProfileCardMenuDeleteButton', async function() {
    profileCardMenuElement.$$('#moreActionsButton').click();
    const menuButtons = profileCardMenuElement.shadowRoot.querySelectorAll(
        '#actionMenu > .dropdown-item');
    menuButtons[menuButtonIndex.DELETE].click();
    assertFalse(profileCardMenuElement.$$('#actionMenu').open);
    assertTrue(profileCardMenuElement.$$('#removeConfirmationDialog').open);
  });

  // Click on the cancel button in the remove confirmation dialog closes the
  // dialog.
  test('RemoveConfirmationDialogCancel', async function() {
    const dialog = profileCardMenuElement.$$('#removeConfirmationDialog');
    dialog.showModal();
    assertTrue(dialog.open);
    dialog.querySelector('.cancel-button').click();
    assertFalse(dialog.open);
    assertEquals(browserProxy.getCallCount('removeProfile'), 0);
  });

  // Click on the delete button in the remove confirmation dialog calls native
  // to remove profile.
  test('RemoveConfirmationDialogDelete', async function() {
    const dialog = profileCardMenuElement.$$('#removeConfirmationDialog');
    dialog.showModal();
    assertTrue(dialog.open);
    dialog.querySelector('.action-button').click();
    await browserProxy.whenCalled('removeProfile');
    webUIListenerCallback('profile-removed', 'profilePath');
    assertFalse(dialog.open);
  });
});
