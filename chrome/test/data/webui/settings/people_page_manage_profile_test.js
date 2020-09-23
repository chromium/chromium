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
      {url: 'fake-icon-1.png', label: 'fake-icon-1'},
      {url: 'fake-icon-2.png', label: 'fake-icon-2', selected: true},
      {url: 'gaia-icon.png', label: 'gaia-icon', isGaiaAvatar: true},
    ]);
  }

  /** @override */
  setProfileIconToGaiaAvatar() {
    this.methodCalled('setProfileIconToGaiaAvatar');
  }

  /** @override */
  setProfileIconToDefaultAvatar(iconUrl) {
    this.methodCalled('setProfileIconToDefaultAvatar', [iconUrl]);
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
    PolymerTest.clearBody();
    manageProfile = createManageProfileElement();
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

  // Tests that the manage profile subpage
  //  - gets and receives all the available icons
  //  - can select a new icon
  test('ManageProfileChangeIcon', function() {
    let items = null;
    return browserProxy.whenCalled('getAvailableIcons')
        .then(function() {
          flush();
          items =
              manageProfile.$.avatarSelector.$['avatar-grid'].querySelectorAll(
                  '.avatar');

          assertFalse(!!manageProfile.profileAvatar);
          assertEquals(3, items.length);
          assertFalse(items[0].classList.contains('iron-selected'));
          assertTrue(items[1].classList.contains('iron-selected'));
          assertFalse(items[2].classList.contains('iron-selected'));

          items[1].click();
          return browserProxy.whenCalled('setProfileIconToDefaultAvatar');
        })
        .then(function(args) {
          assertEquals('fake-icon-2.png', args[0]);

          items[2].click();
          return browserProxy.whenCalled('setProfileIconToGaiaAvatar');
        });
  });

  test('ManageProfileChangeName', function() {
    const nameField = manageProfile.$.name;
    assertTrue(!!nameField);
    assertFalse(!!nameField.disabled);
    assertEquals('.*\\S.*', nameField.pattern);

    assertEquals('Initial Fake Name', nameField.value);

    nameField.value = 'New Name';
    nameField.fire('change');

    return browserProxy.whenCalled('setProfileName').then(function(args) {
      assertEquals('New Name', args[0]);
    });
  });

  test('ProfileNameIsDisabledForSupervisedUser', function() {
    manageProfile.syncStatus = {supervisedUser: true, childUser: false};

    const nameField = manageProfile.$.name;
    assertTrue(!!nameField);

    // Name field should be disabled for legacy supervised users.
    assertTrue(!!nameField.disabled);
  });

  // Tests profile name updates pushed from the browser.
  test('ManageProfileNameUpdated', function() {
    const nameField = manageProfile.$.name;
    assertTrue(!!nameField);

    return browserProxy.whenCalled('getAvailableIcons').then(function() {
      manageProfile.profileName = 'New Name From Browser';

      flush();

      assertEquals('New Name From Browser', nameField.value);
    });
  });

  // Tests profile shortcut toggle is hidden if profile shortcuts feature is
  // disabled.
  test('ManageProfileShortcutToggleHidden', function() {
    const hasShortcutToggle = manageProfile.$$('#hasShortcutToggle');
    assertFalse(!!hasShortcutToggle);
  });


  // Tests that the theme selector is hidden if profile colors feature is
  // disabled.
  test('ProfileThemeSelectorHidden', function() {
    assertFalse(!!manageProfile.$$('#themeSelector'));
  });

  // Tests that the theme selector is visible if profile colors feature is
  // enabled.
  test('ProfileThemeSelectorVisible', function() {
    // Recreate a manage profile element with overridden loadTimeData.
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      profileThemeSelectorEnabled: true,
    });
    manageProfile = createManageProfileElement();
    flush();

    assertTrue(!!manageProfile.$$('#themeSelector'));
  });
});

suite('ManageProfileTestsProfileShortcutsEnabled', function() {
  let manageProfile = null;
  let browserProxy = null;

  setup(function() {
    loadTimeData.overrideValues({
      profileShortcutsEnabled: true,
    });

    browserProxy = new TestManageProfileBrowserProxy();
    ManageProfileBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    manageProfile = document.createElement('settings-manage-profile');
    manageProfile.profileIconUrl = 'fake-icon-1.png';
    manageProfile.profileName = 'Initial Fake Name';
    manageProfile.syncStatus = {supervisedUser: false, childUser: false};
    document.body.appendChild(manageProfile);
  });

  teardown(function() {
    manageProfile.remove();
  });

  // Tests profile shortcut toggle is visible and toggling it removes and
  // creates the profile shortcut respectively.
  test('ManageProfileShortcutToggle', function() {
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    flush();

    assertFalse(!!manageProfile.$$('#hasShortcutToggle'));

    return browserProxy.whenCalled('getProfileShortcutStatus').then(function() {
      flush();

      const hasShortcutToggle = manageProfile.$$('#hasShortcutToggle');
      assertTrue(!!hasShortcutToggle);

      // The profile shortcut toggle is checked.
      assertTrue(hasShortcutToggle.checked);

      // Simulate tapping the profile shortcut toggle.
      hasShortcutToggle.click();
      return browserProxy.whenCalled('removeProfileShortcut').then(function() {
        flush();

        // The profile shortcut toggle is checked.
        assertFalse(hasShortcutToggle.checked);

        // Simulate tapping the profile shortcut toggle.
        hasShortcutToggle.click();
        return browserProxy.whenCalled('addProfileShortcut');
      });
    });
  });

  // Tests profile shortcut toggle is visible and toggled off when no
  // profile shortcut is found.
  test('ManageProfileShortcutToggle', function() {
    browserProxy.setProfileShortcutStatus(
        ProfileShortcutStatus.PROFILE_SHORTCUT_NOT_FOUND);

    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    flush();

    assertFalse(!!manageProfile.$$('#hasShortcutToggle'));

    return browserProxy.whenCalled('getProfileShortcutStatus').then(function() {
      flush();

      const hasShortcutToggle = manageProfile.$$('#hasShortcutToggle');
      assertTrue(!!hasShortcutToggle);

      assertFalse(hasShortcutToggle.checked);
    });
  });

  // Tests the case when the profile shortcut setting is hidden. This can
  // occur in the single profile case.
  test('ManageProfileShortcutSettingHIdden', function() {
    browserProxy.setProfileShortcutStatus(
        ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN);

    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    flush();

    assertFalse(!!manageProfile.$$('#hasShortcutToggle'));

    return browserProxy.whenCalled('getProfileShortcutStatus').then(function() {
      flush();

      assertFalse(!!manageProfile.$$('#hasShortcutToggle'));
    });
  });
});
