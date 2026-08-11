// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {ManageProfileBrowserProxy, SettingsManageProfileElement} from 'chrome://settings/lazy_load.js';
import {ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from 'chrome://settings/lazy_load.js';
import type {CrToggleElement} from 'chrome://settings/settings.js';
import {loadTimeData, ProfileInfoBrowserProxyImpl, resetRouterForTesting, Router, routes, StatusAction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';

class TestManageProfileBrowserProxy extends TestBrowserProxy implements
    ManageProfileBrowserProxy {
  private profileShortcutStatus_: ProfileShortcutStatus =
      ProfileShortcutStatus.PROFILE_SHORTCUT_FOUND;

  constructor() {
    super([
      'getAvailableIcons',
      'setProfileIconToGaiaAvatar',
      'setProfileIconToDefaultAvatar',
      'setProfileName',
      'getProfileShortcutStatus',
      'addProfileShortcut',
      'removeProfileShortcut',
    ]);
  }

  setProfileShortcutStatus(status: ProfileShortcutStatus) {
    this.profileShortcutStatus_ = status;
  }

  getAvailableIcons() {
    this.methodCalled('getAvailableIcons');
    return Promise.resolve([
      {
        url: 'fake-icon-1.png',
        label: 'fake-icon-1',
        index: 1,
        isGaiaAvatar: false,
        selected: false,
      },
      {
        url: 'fake-icon-2.png',
        label: 'fake-icon-2',
        index: 2,
        selected: true,
        isGaiaAvatar: false,
      },
      {
        url: 'gaia-icon.png',
        label: 'gaia-icon',
        index: 3,
        isGaiaAvatar: true,
        selected: false,
      },
    ]);
  }

  setProfileIconToGaiaAvatar() {
    this.methodCalled('setProfileIconToGaiaAvatar');
  }

  setProfileIconToDefaultAvatar(index: number) {
    this.methodCalled('setProfileIconToDefaultAvatar', [index]);
  }

  setProfileName(name: string) {
    this.methodCalled('setProfileName', [name]);
  }

  getProfileShortcutStatus() {
    this.methodCalled('getProfileShortcutStatus');
    return Promise.resolve(this.profileShortcutStatus_);
  }

  addProfileShortcut() {
    this.methodCalled('addProfileShortcut');
  }

  removeProfileShortcut() {
    this.methodCalled('removeProfileShortcut');
  }
}

suite('ManageProfileTests', function() {
  let manageProfile: SettingsManageProfileElement;
  let browserProxy: TestManageProfileBrowserProxy;
  let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;

  setup(async function() {
    browserProxy = new TestManageProfileBrowserProxy();
    ManageProfileBrowserProxyImpl.setInstance(browserProxy);
    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    profileInfoBrowserProxy.fakeProfileInfo = {
      name: 'Initial Fake Name',
      iconUrl: '',
    };
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({profileShortcutsEnabled: false});
    resetRouterForTesting();
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    manageProfile = await createManageProfileElement();
  });

  teardown(function() {
    manageProfile.remove();
  });

  async function createManageProfileElement():
      Promise<SettingsManageProfileElement> {
    const element = document.createElement('settings-manage-profile');
    document.body.appendChild(element);
    webUIListenerCallback('sync-status-changed', {
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    });
    webUIListenerCallback(
        'profile-info-changed', {name: 'Initial Fake Name', iconUrl: ''});
    await microtasksFinished();
    return element;
  }

  // Tests that the manage profile subpage
  //  - gets and receives all the available icons
  //  - can select a new icon
  test('ManageProfileChangeIcon', async function() {
    let items = null;
    await browserProxy.whenCalled('getAvailableIcons');
    await microtasksFinished();
    items =
        manageProfile.shadowRoot.querySelector(
                                    'cr-profile-avatar-selector')!.shadowRoot
            .querySelector('#avatar-grid')!.querySelectorAll<HTMLElement>(
                '.avatar-container > .avatar');

    assertEquals(3, items.length);
    assertFalse(items[0]!.parentElement!.classList.contains('iron-selected'));
    assertTrue(items[1]!.parentElement!.classList.contains('iron-selected'));
    assertFalse(items[2]!.parentElement!.classList.contains('iron-selected'));

    items[1]!.click();
    await microtasksFinished();
    const args = await browserProxy.whenCalled('setProfileIconToDefaultAvatar');
    assertEquals(2, args[0]);

    items[2]!.click();
    await microtasksFinished();
    await browserProxy.whenCalled('setProfileIconToGaiaAvatar');
  });

  test('ManageProfileChangeName', async function() {
    const nameInput = manageProfile.$.nameInput;
    assertTrue(!!nameInput);
    assertFalse(nameInput.disabled);
    assertEquals('.*\\S.*', nameInput.pattern);

    assertEquals('Initial Fake Name', nameInput.value);
    // No policy indicator is shown.
    const policyIndicator =
        nameInput.shadowRoot.querySelector<HTMLElement>('#policyIcon');
    assertEquals(policyIndicator, null);

    nameInput.value = 'New Name';
    nameInput.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));

    const args = await browserProxy.whenCalled('setProfileName');
    assertEquals('New Name', args[0]);
  });

  // Tests profile name updates pushed from the browser.
  test('ManageProfileNameUpdated', async function() {
    await browserProxy.whenCalled('getAvailableIcons');
    webUIListenerCallback(
        'profile-info-changed', {name: 'New Name From Browser', iconUrl: ''});
    await microtasksFinished();
    assertEquals('New Name From Browser', manageProfile.$.nameInput.value);
  });

  // Tests profile name is not editable for work profile.
  test('ManageProfileNameDisabledForEnterprise', async function() {
    loadTimeData.overrideValues({hasEnterpriseLabel: true});
    manageProfile = await createManageProfileElement();
    const nameInput = manageProfile.$.nameInput;
    assertTrue(nameInput.disabled);
    assertEquals('Initial Fake Name', nameInput.value);

    // The policy indicator is shown.
    const policyIndicator =
        nameInput.shadowRoot.querySelector<HTMLElement>('#policyIcon');
    assertFalse(!!policyIndicator && policyIndicator.hidden);
  });

  // Tests that the theme selector is visible.
  test('ThemeColorPicker', async function() {
    manageProfile = await createManageProfileElement();
    assertTrue(isVisible(
        manageProfile.shadowRoot.querySelector('cr-theme-color-picker')));
  });

  // Tests profile shortcut toggle is hidden if profile shortcuts feature is
  // disabled.
  test('ManageProfileShortcutToggleHidden', function() {
    const hasShortcutToggle =
        manageProfile.shadowRoot.querySelector('#hasShortcutToggle');
    assertFalse(!!hasShortcutToggle);
  });

  // Tests profile shortcut toggle is visible and toggling it removes and
  // creates the profile shortcut respectively.
  test('ManageProfileShortcutToggle', async function() {
    resetRouterForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({profileShortcutsEnabled: true});
    manageProfile = await createManageProfileElement();

    assertFalse(!!manageProfile.shadowRoot.querySelector('#hasShortcutToggle'));

    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    await browserProxy.whenCalled('getProfileShortcutStatus');
    await microtasksFinished();

    const hasShortcutToggle =
        manageProfile.shadowRoot.querySelector<CrToggleElement>(
            '#hasShortcutToggle');
    assertTrue(!!hasShortcutToggle);

    // The profile shortcut toggle is checked.
    assertTrue(hasShortcutToggle.checked);

    // Simulate tapping the profile shortcut toggle.
    hasShortcutToggle.click();
    await browserProxy.whenCalled('removeProfileShortcut');

    await microtasksFinished();

    // The profile shortcut toggle is unchecked.
    assertFalse(hasShortcutToggle.checked);

    // Simulate tapping the profile shortcut toggle.
    hasShortcutToggle.click();
    await browserProxy.whenCalled('addProfileShortcut');
  });

  // Tests profile shortcut toggle is visible and toggled off when no
  // profile shortcut is found.
  test('ManageProfileShortcutToggleShortcutNotFound', async function() {
    resetRouterForTesting();
    browserProxy.setProfileShortcutStatus(
        ProfileShortcutStatus.PROFILE_SHORTCUT_NOT_FOUND);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({profileShortcutsEnabled: true});
    manageProfile = await createManageProfileElement();

    assertFalse(!!manageProfile.shadowRoot.querySelector('#hasShortcutToggle'));

    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    await browserProxy.whenCalled('getProfileShortcutStatus');
    await microtasksFinished();

    const hasShortcutToggle =
        manageProfile.shadowRoot.querySelector<CrToggleElement>(
            '#hasShortcutToggle');
    assertTrue(!!hasShortcutToggle);

    assertFalse(hasShortcutToggle.checked);
  });

  // Tests the case when the profile shortcut setting is hidden. This can
  // occur in the single profile case.
  test('ManageProfileShortcutSettingHidden', async function() {
    resetRouterForTesting();
    browserProxy.setProfileShortcutStatus(
        ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({profileShortcutsEnabled: true});
    manageProfile = await createManageProfileElement();

    assertFalse(!!manageProfile.shadowRoot.querySelector('#hasShortcutToggle'));

    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    await browserProxy.whenCalled('getProfileShortcutStatus');
    await microtasksFinished();

    assertFalse(!!manageProfile.shadowRoot.querySelector('#hasShortcutToggle'));
  });
});
