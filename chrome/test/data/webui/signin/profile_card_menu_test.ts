// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';

import type {ProfileCardMenuElement, ProfileState, Statistics, StatisticsResult} from 'chrome://profile-picker/profile_picker.js';
import {ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

enum MenuButtonIndex {
  CUSTOMIZE = 0,
  DELETE = 1,
}

suite('ProfileCardMenuTest', function() {
  let profileCardMenuElement: ProfileCardMenuElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  const statisticsDataTypes: string[] =
      ['BrowsingHistory', 'Passwords', 'Bookmarks', 'Autofill'];

  setup(async function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    profileCardMenuElement = document.createElement('profile-card-menu');
    const testProfileState: ProfileState = {
      profilePath: `profilePath`,
      localProfileName: `profile`,
      isSyncing: true,
      needsSignin: false,
      gaiaName: `User`,
      userName: `User@gmail.com`,
      avatarIcon: `AvatarUrl`,
      avatarBadge: ``,
      // <if expr="chromeos_lacros">
      isPrimaryLacrosProfile: false,
      // </if>
    };
    profileCardMenuElement.profileState = testProfileState;
    document.body.appendChild(profileCardMenuElement);
  });

  // Checks basic layout of the action menu.
  test('ProfileCardMenuActionMenu', async function() {
    assertFalse(profileCardMenuElement.$.actionMenu.open);
    assertFalse(profileCardMenuElement.$.removeConfirmationDialog.open);
    profileCardMenuElement.$.moreActionsButton.click();
    assertTrue(profileCardMenuElement.$.actionMenu.open);
    assertFalse(profileCardMenuElement.$.removeConfirmationDialog.open);
    const menuButtons = profileCardMenuElement.shadowRoot!.querySelectorAll(
        '#actionMenu > .dropdown-item');
    assertEquals(menuButtons.length, 2);
  });

  // Click on the customize profile menu item calls native to open the profile
  // settings page.
  test('ProfileCardMenuCustomizeButton', async function() {
    profileCardMenuElement.$.moreActionsButton.click();
    const menuButtons =
        profileCardMenuElement.$.actionMenu.querySelectorAll<HTMLButtonElement>(
            '.dropdown-item');
    menuButtons[MenuButtonIndex.CUSTOMIZE]!.click();
    await browserProxy.whenCalled('openManageProfileSettingsSubPage');
    assertFalse(profileCardMenuElement.$.actionMenu.open);
    assertFalse(profileCardMenuElement.$.removeConfirmationDialog.open);
  });

  // Click on the delete profile menu item opens the remove confirmation dialog.
  test('ProfileCardMenuDeleteButton', async function() {
    profileCardMenuElement.$.moreActionsButton.click();
    const menuButtons =
        profileCardMenuElement.shadowRoot!.querySelectorAll<HTMLButtonElement>(
            '#actionMenu > .dropdown-item');
    menuButtons[MenuButtonIndex.DELETE]!.click();
    assertFalse(profileCardMenuElement.$.actionMenu.open);
    assertTrue(profileCardMenuElement.$.removeConfirmationDialog.open);
  });

  // Click on the cancel button in the remove confirmation dialog closes the
  // dialog.
  test('RemoveConfirmationDialogCancel', async function() {
    const dialog = profileCardMenuElement.$.removeConfirmationDialog;
    dialog.showModal();
    assertTrue(dialog.open);
    dialog.querySelector<HTMLElement>('.cancel-button')!.click();
    assertFalse(dialog.open);
    assertEquals(browserProxy.getCallCount('closeProfileStatistics'), 1);
    assertEquals(browserProxy.getCallCount('removeProfile'), 0);
  });

  // Click on the delete button in the remove confirmation dialog calls native
  // to remove profile.
  test('RemoveConfirmationDialogDelete', async function() {
    const dialog = profileCardMenuElement.$.removeConfirmationDialog;
    dialog.showModal();
    assertTrue(dialog.open);
    dialog.querySelector<HTMLElement>('.action-button')!.click();
    await browserProxy.whenCalled('removeProfile');
    webUIListenerCallback('profile-removed', 'profilePath');
    assertFalse(dialog.open);
    assertEquals(browserProxy.getCallCount('closeProfileStatistics'), 0);
  });

  // The profile info in the remove confirmation dialog is displayed correctly.
  test('RemoveConfirmationDialogProfileCard', async function() {
    const dialog = profileCardMenuElement.$.removeConfirmationDialog;
    dialog.showModal();
    assertTrue(dialog.open);

    assertEquals(
        dialog.querySelector<HTMLElement>('#profileName')!.innerText,
        'profile');
    assertEquals(
        dialog.querySelector<HTMLElement>('#gaiaName')!.innerText, 'User');

    const updatedProfileState: ProfileState =
        Object.assign({}, profileCardMenuElement.profileState);
    updatedProfileState.localProfileName = 'updatedProfile';
    updatedProfileState.gaiaName = 'updatedUser';
    profileCardMenuElement.profileState = updatedProfileState;
    await microtasksFinished();

    assertEquals(
        dialog.querySelector<HTMLElement>('#profileName')!.innerText,
        'updatedProfile');
    assertEquals(
        dialog.querySelector<HTMLElement>('#gaiaName')!.innerText,
        'updatedUser');
  });

  // The profile statistics in the remove confirmation dialog are displayed
  // correctly.
  test('RemoveConfirmationDialogStatistics', async function() {
    const dialog = profileCardMenuElement.$.removeConfirmationDialog;
    dialog.showModal();
    assertTrue(dialog.open);

    const statistics: Statistics = {
      BrowsingHistory: 1,
      Passwords: 2,
      Bookmarks: 3,
      Autofill: 4,
    };
    const statisticsResult: StatisticsResult = {
      profilePath: 'profilePath',
      statistics: statistics,
    };
    webUIListenerCallback('profile-statistics-received', statisticsResult);
    await microtasksFinished();

    const statisticsCountElements =
        dialog.querySelector('.statistics')!.querySelectorAll<HTMLElement>(
            '.count');
    for (let i = 0; i < statisticsDataTypes.length; i++) {
      assertEquals(
          statisticsCountElements[i]!.innerText,
          statistics[statisticsDataTypes[i] as keyof Statistics].toString());
    }
  });

  // The profile statistics of another profile aren't displayed.
  test('RemoveConfirmationDialogStatisticsWrongProfile', async function() {
    const dialog = profileCardMenuElement.$.removeConfirmationDialog;
    dialog.showModal();
    assertTrue(dialog.open);

    const statistics: Statistics = {
      BrowsingHistory: 1,
      Passwords: 0,
      Autofill: 0,
      Bookmarks: 0,
    };
    const statisticsResult: StatisticsResult = {
      profilePath: 'anotherProfilePath',
      statistics: statistics,
    };
    webUIListenerCallback('profile-statistics-received', statisticsResult);

    const statisticsCountElements =
        dialog.querySelector('.statistics')!.querySelectorAll<HTMLElement>(
            '.count');
    assertNotEquals(
        statisticsCountElements[statisticsDataTypes.indexOf('BrowsingHistory')]!
            .innerText,
        '1');
  });
});

// <if expr="chromeos_lacros">
suite('ProfileCardMenuLacrosTest', function() {
  let primaryProfileCardMenuElement: ProfileCardMenuElement;
  let secondaryProfileCardMenuElement: ProfileCardMenuElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  setup(async function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    primaryProfileCardMenuElement = document.createElement('profile-card-menu');
    const testPrimaryProfileState: ProfileState = {
      profilePath: `primaryProfilePath`,
      localProfileName: `profile`,
      isSyncing: true,
      needsSignin: false,
      gaiaName: `User`,
      userName: `User@gmail.com`,
      avatarIcon: `AvatarUrl`,
      avatarBadge: `cr:domain`,
      isPrimaryLacrosProfile: true,
    };
    primaryProfileCardMenuElement.profileState = testPrimaryProfileState;
    document.body.appendChild(primaryProfileCardMenuElement);
    secondaryProfileCardMenuElement =
        document.createElement('profile-card-menu');
    const testSecondaryProfileState: ProfileState = {
      profilePath: `secondaryProfilePath`,
      localProfileName: `profile`,
      isSyncing: true,
      needsSignin: false,
      gaiaName: `User2`,
      userName: `User2@gmail.com`,
      avatarIcon: `AvatarUrl`,
      avatarBadge: ``,
      isPrimaryLacrosProfile: false,
    };
    secondaryProfileCardMenuElement.profileState = testSecondaryProfileState;
    document.body.appendChild(secondaryProfileCardMenuElement);
  });

  // The primary profile cannot be deleted in Lacros. The delete button should
  // just open a notification about that.
  test('PrimaryProfileCannotBeDeleted', async function() {
    primaryProfileCardMenuElement.$.moreActionsButton.click();
    const menuButtons = primaryProfileCardMenuElement.shadowRoot!
                            .querySelectorAll<HTMLButtonElement>(
                                '#actionMenu > .dropdown-item');
    assertFalse(menuButtons[MenuButtonIndex.DELETE]!.disabled);
    menuButtons[MenuButtonIndex.DELETE]!.click();
    assertFalse(primaryProfileCardMenuElement.$.actionMenu.open);
    const dialog =
        primaryProfileCardMenuElement.$.removePrimaryLacrosProfileDialog;
    assertTrue(dialog.open);
    dialog.querySelector<HTMLElement>('.action-button')!.click();
    await microtasksFinished();
    assertFalse(dialog.open);
  });

  // All other profiles can be deleted as normal.
  test('SecondaryProfileCanBeDeleted', async function() {
    secondaryProfileCardMenuElement.$.moreActionsButton.click();
    const menuButtons = secondaryProfileCardMenuElement.shadowRoot!
                            .querySelectorAll<HTMLButtonElement>(
                                '#actionMenu > .dropdown-item');
    assertFalse(menuButtons[MenuButtonIndex.DELETE]!.disabled);
    menuButtons[MenuButtonIndex.DELETE]!.click();
    assertFalse(secondaryProfileCardMenuElement.$.actionMenu.open);
    assertTrue(secondaryProfileCardMenuElement.$.removeConfirmationDialog.open);
  });

  // Check that the confirmation dialog has a clickable link.
  test('RemoveConfirmationDialogLink', async function() {
    const dialog = secondaryProfileCardMenuElement.$.removeConfirmationDialog;
    dialog.showModal();
    assertTrue(dialog.open);

    const settingsLink = dialog.querySelector<HTMLElement>(
        '#removeWarningHeader a[is="action-link"]');
    settingsLink!.click();
    await browserProxy.whenCalled('openAshAccountSettingsPage');
  });

});
// </if>
