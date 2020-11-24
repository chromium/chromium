// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/** @implements {ManageProfileBrowserProxy} */
class TestManageProfileBrowserProxy extends TestBrowserProxy {
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

    /** @private {!ProfileShortcutStatus} */
    this.profileShortcutStatus_ = ProfileShortcutStatus.PROFILE_SHORTCUT_FOUND;
  }

  /** @param {!ProfileShortcutStatus} status */
  setProfileShortcutStatus(status) {
    this.profileShortcutStatus_ = status;
  }

  /** @override */
  getAvailableIcons() {
    this.methodCalled('getAvailableIcons');
    return Promise.resolve([
      {url: 'fake-icon-1.png', label: 'fake-icon-1', index: 1},
      {url: 'fake-icon-2.png', label: 'fake-icon-2', index: 2, selected: true},
      {url: 'gaia-icon.png', label: 'gaia-icon', isGaiaAvatar: true},
    ]);
  }

  /** @override */
  setProfileIconToGaiaAvatar() {
    this.methodCalled('setProfileIconToGaiaAvatar');
  }

  /** @override */
  setProfileIconToDefaultAvatar(index) {
    this.methodCalled('setProfileIconToDefaultAvatar', [index]);
  }

  /** @override */
  setProfileName(name) {
    this.methodCalled('setProfileName', [name]);
  }

  /** @override */
  getProfileShortcutStatus() {
    this.methodCalled('getProfileShortcutStatus');
    return Promise.resolve(this.profileShortcutStatus_);
  }

  /** @override */
  addProfileShortcut() {
    this.methodCalled('addProfileShortcut');
  }

  /** @override */
  removeProfileShortcut() {
    this.methodCalled('removeProfileShortcut');
  }
}

suite('ManageProfileTests', function() {
  let manageProfile = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestManageProfileBrowserProxy();
    ManageProfileBrowserProxyImpl.instance_ = browserProxy;
    setFlags({newProfilePicker: false, profileShortcutsEnabled: false});
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
  });

  teardown(function() {
    manageProfile.remove();
  });

  function createManageProfileElement() {
    const manageProfileElement =
        document.createElement('settings-manage-profile');
    manageProfileElement.profileIconUrl = 'fake-icon-1.png';
    manageProfileElement.profileName = 'Initial Fake Name';
    manageProfileElement.syncStatus = {supervisedUser: false, childUser: false};
    document.body.appendChild(manageProfileElement);
    return manageProfileElement;
  }

  /**
   * Recreates manage profile element with overridden loadTimeData.
   * @param {Object} replacements The dictionary object of keys to replace.
   */
  function setFlags(replacements) {
    PolymerTest.clearBody();
    loadTimeData.overrideValues(replacements);
    manageProfile = createManageProfileElement();
    flush();
  }

  // Tests that the manage profile subpage
  //  - gets and receives all the available icons
  //  - can select a new icon
  test('ManageProfileChangeIcon', async function() {
    let items = null;
    async function changeIcon() {
      await browserProxy.whenCalled('getAvailableIcons');
      flush();
      items = manageProfile.$$('cr-profile-avatar-selector')
                  .$$('#avatar-grid')
                  .querySelectorAll('.avatar');

      assertFalse(!!manageProfile.profileAvatar);
      assertEquals(3, items.length);
      assertFalse(items[0].classList.contains('iron-selected'));
      assertTrue(items[1].classList.contains('iron-selected'));
      assertFalse(items[2].classList.contains('iron-selected'));

      items[1].click();
      const args =
          await browserProxy.whenCalled('setProfileIconToDefaultAvatar');
      assertEquals(2, args[0]);

      items[2].click();
      await browserProxy.whenCalled('setProfileIconToGaiaAvatar');
    }
    await changeIcon();
    browserProxy.reset();
    setFlags({newProfilePicker: true});
    await changeIcon();
  });

  test('ManageProfileChangeName', async function() {
    async function changeName() {
      const nameField = manageProfile.$$('#name');
      assertTrue(!!nameField);
      assertFalse(!!nameField.disabled);
      assertEquals('.*\\S.*', nameField.pattern);

      assertEquals('Initial Fake Name', nameField.value);

      nameField.value = 'New Name';
      nameField.fire('change');

      const args = await browserProxy.whenCalled('setProfileName');
      assertEquals('New Name', args[0]);
    }
    await changeName();
    browserProxy.resetResolver('setProfileName');
    setFlags({newProfilePicker: true});
    await changeName();
  });

  test('ProfileNameIsDisabledForSupervisedUser', function() {
    function profileNameDisabledForSupervisedUser() {
      manageProfile.syncStatus = {supervisedUser: true, childUser: false};

      const nameField = manageProfile.$$('#name');
      assertTrue(!!nameField);

      // Name field should be disabled for legacy supervised users.
      assertTrue(!!nameField.disabled);
    }
    profileNameDisabledForSupervisedUser();
    setFlags({newProfilePicker: true});
    profileNameDisabledForSupervisedUser();
  });

  // Tests profile name updates pushed from the browser.
  test('ManageProfileNameUpdated', async function() {
    async function profileNameUpdated() {
      const nameField = manageProfile.$$('#name');
      assertTrue(!!nameField);

      await browserProxy.whenCalled('getAvailableIcons');
      manageProfile.profileName = 'New Name From Browser';

      flush();

      assertEquals('New Name From Browser', nameField.value);
    }
    await profileNameUpdated();
    browserProxy.resetResolver('getAvailableIcons');
    setFlags({newProfilePicker: true});
    await profileNameUpdated();
  });

  // Tests profile shortcut toggle is hidden if profile shortcuts feature is
  // disabled.
  test('ManageProfileShortcutToggleHidden', function() {
    const hasShortcutToggle = manageProfile.$$('#hasShortcutToggle');
    assertFalse(!!hasShortcutToggle);
  });


  // Tests that the theme selector is visible if new manage profile design
  // feature is enabled and hidden otherwise.
  test('ProfileThemeSelector', function() {
    assertFalse(!!manageProfile.$$('#themeSelector'));

    // Recreate a manage profile element with overridden loadTimeData.
    setFlags({newProfilePicker: true});

    assertTrue(!!manageProfile.$$('#themeSelector'));
  });

  // Tests profile shortcut toggle is visible and toggling it removes and
  // creates the profile shortcut respectively.
  test('ManageProfileShortcutToggle', async function() {
    async function profileShortcutToggle() {
      assertFalse(!!manageProfile.$$('#hasShortcutToggle'));
      await browserProxy.whenCalled('getProfileShortcutStatus');

      flush();

      const hasShortcutToggle = manageProfile.$$('#hasShortcutToggle');
      assertTrue(!!hasShortcutToggle);

      // The profile shortcut toggle is checked.
      assertTrue(hasShortcutToggle.checked);

      // Simulate tapping the profile shortcut toggle.
      hasShortcutToggle.click();
      await browserProxy.whenCalled('removeProfileShortcut');

      flush();

      // The profile shortcut toggle is checked.
      assertFalse(hasShortcutToggle.checked);

      // Simulate tapping the profile shortcut toggle.
      hasShortcutToggle.click();
      return browserProxy.whenCalled('addProfileShortcut');
    }
    setFlags({newProfilePicker: false, profileShortcutsEnabled: true});
    await profileShortcutToggle();
    browserProxy.reset();
    setFlags({newProfilePicker: true, profileShortcutsEnabled: true});
    await profileShortcutToggle();
  });

  // Tests profile shortcut toggle is visible and toggled off when no
  // profile shortcut is found.
  test('ManageProfileShortcutToggleShortcutNotFound', async function() {
    browserProxy.setProfileShortcutStatus(
        ProfileShortcutStatus.PROFILE_SHORTCUT_NOT_FOUND);

    async function profileShortcutToggleShortcutNotFound() {
      assertFalse(!!manageProfile.$$('#hasShortcutToggle'));
      await browserProxy.whenCalled('getProfileShortcutStatus');

      flush();

      const hasShortcutToggle = manageProfile.$$('#hasShortcutToggle');
      assertTrue(!!hasShortcutToggle);

      assertFalse(hasShortcutToggle.checked);
    }
    setFlags({newProfilePicker: false, profileShortcutsEnabled: true});
    await profileShortcutToggleShortcutNotFound();
    browserProxy.resetResolver('getProfileShortcutStatus');
    setFlags({newProfilePicker: true, profileShortcutsEnabled: true});
    await profileShortcutToggleShortcutNotFound();
  });

  // Tests the case when the profile shortcut setting is hidden. This can
  // occur in the single profile case.
  test('ManageProfileShortcutSettingHidden', async function() {
    browserProxy.setProfileShortcutStatus(
        ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN);

    async function profileShortcutSettingHidden() {
      assertFalse(!!manageProfile.$$('#hasShortcutToggle'));

      await browserProxy.whenCalled('getProfileShortcutStatus');

      flush();

      assertFalse(!!manageProfile.$$('#hasShortcutToggle'));
    }
    setFlags({newProfilePicker: false, profileShortcutsEnabled: true});
    await profileShortcutSettingHidden();
    browserProxy.resetResolver('getProfileShortcutStatus');
    setFlags({newProfilePicker: true, profileShortcutsEnabled: true});
    await profileShortcutSettingHidden();
  });
});
