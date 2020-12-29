// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ManageProfilesBrowserProxyImpl, NavigationBehavior, ProfileState, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {flushTasks, waitBeforeNextRender} from '../test_util.m.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';


suite('ProfilePickerMainViewTest', function() {
  /** @type {!ProfilePickerMainViewElement} */
  let mainViewElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  let navigationElement;
  suiteSetup(function() {
    Polymer({
      is: 'navigation-element',

      behaviors: [NavigationBehavior],

      ready: function() {
        this.reset();
      },

      /**
       * @param {Routes} route
       * @param {string} step
       */
      onRouteChange: function(route, step) {
        this.changeCalled = true;
        this.route = route;
      },

      reset: function() {
        this.changeCalled = false;
        this.route = '';
      }
    });
  });

  function resetTest() {
    document.body.innerHTML = '';
    navigationElement = document.createElement('navigation-element');
    document.body.appendChild(navigationElement);
    mainViewElement = /** @type {!ProfilePickerMainViewElement} */ (
        document.createElement('profile-picker-main-view'));
    document.body.appendChild(mainViewElement);
    return waitBeforeNextRender(mainViewElement);
  }

  function resetPolicies() {
    // This is necessary as |loadTimeData| state leaks between tests.
    // Any load time data manipulated by the tests needs to be reset here.
    loadTimeData.overrideValues({
      isGuestModeEnabled: true,
      isProfileCreationAllowed: true,
      disableAskOnStartup: false
    });
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.instance_ = browserProxy;
    resetPolicies();
    return resetTest();
  });

  /**
   * @param {number} n Indicates the desired number of profiles.
   * @return {!Array<!ProfileState>} Array of profiles.
   */
  function generateProfilesList(n) {
    return Array(n)
        .fill(0)
        .map((x, i) => i % 2 === 0)
        .map((sync, i) => ({
               profilePath: `profilePath${i}`,
               localProfileName: `profile${i}`,
               isSyncing: sync,
               needsSignin: false,
               gaiaName: sync ? `User${i}` : '',
               userName: sync ? `User${i}@gmail.com` : '',
               isManaged: i % 4 === 0,
               avatarIcon: `AvatarUrl-${i}`,
             }));
  }

  /**
   * @param {!Array<!ProfileState>} expectedProfiles
   * @param {!Array<!ProfileCardElement>} Array of profiles.
   */
  async function verifyProfileCard(expectedProfiles, profiles) {
    assertEquals(expectedProfiles.length, profiles.length);
    for (let i = 0; i < expectedProfiles.length; i++) {
      assertTrue(!!profiles[i].$$('profile-card-menu'));
      profiles[i].$$('cr-button').click();
      await browserProxy.whenCalled('launchSelectedProfile');
      assertEquals(
          profiles[i].$$('#forceSigninContainer').hidden,
          !expectedProfiles[i].needsSignin);

      const gaiaName = profiles[i].$$('#gaiaName');
      assertEquals(gaiaName.hidden, expectedProfiles[i].needsSignin);
      assertEquals(gaiaName.innerText.trim(), expectedProfiles[i].gaiaName);

      assertEquals(
          profiles[i].$$('#nameInput').value,
          expectedProfiles[i].localProfileName);
      assertEquals(
          profiles[i].$$('#iconContainer').hidden,
          !expectedProfiles[i].isManaged);
      assertEquals(
          (profiles[i].$$('.profile-avatar').src).split('/').pop(),
          expectedProfiles[i].avatarIcon);
    }
  }

  test('MainViewWithDefaultPolicies', async function() {
    assertTrue(navigationElement.changeCalled);
    assertEquals(navigationElement.route, Routes.MAIN);
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$$('#wrapper').hidden);
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
    const profiles = generateProfilesList(6);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    // Profiles list defined.
    assertTrue(!mainViewElement.$$('#wrapper').hidden);
    assertTrue(!mainViewElement.$$('cr-checkbox').hidden);
    assertTrue(mainViewElement.$$('cr-checkbox').checked);
    // Verify profile card.
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    // Browse as guest.
    assertTrue(!!mainViewElement.$$('#browseAsGuestButton'));
    mainViewElement.$$('#browseAsGuestButton').click();
    await browserProxy.whenCalled('launchGuestProfile');
    // Ask when chrome opens.
    mainViewElement.$$('cr-checkbox').click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$$('cr-checkbox').checked);
    // Update profile data.
    profiles[1] = profiles[4];
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    // Profiles update on remove.
    webUIListenerCallback('profile-removed', profiles[3].profilePath);
    profiles.splice(3, 1);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
  });

  test('EditLocalProfileName', async function() {
    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    const localProfileName =
        mainViewElement.shadowRoot.querySelector('profile-card')
            .$$('#nameInput');
    assertEquals(localProfileName.value, profiles[0].localProfileName);

    // Set to valid profile name.
    localProfileName.value = 'Alice';
    localProfileName.fire('change');
    const args = await browserProxy.whenCalled('setProfileName');
    assertEquals(args[0], profiles[0].profilePath);
    assertEquals(args[1], 'Alice');
    assertEquals(localProfileName.value, 'Alice');

    // Set to invalid profile name
    localProfileName.value = '';
    assertTrue(localProfileName.invalid);
  });

  test('GuestModeDisabled', async function() {
    loadTimeData.overrideValues({
      isGuestModeEnabled: false,
    });
    resetTest();
    assertEquals(
        mainViewElement.$$('#browseAsGuestButton').style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    assertEquals(
        mainViewElement.$$('#browseAsGuestButton').style.display, 'none');
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    resetTest();
    assertEquals(mainViewElement.$$('#addProfile').style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    navigationElement.reset();
    assertEquals(mainViewElement.$$('#addProfile').style.display, 'none');
    mainViewElement.$$('#addProfile').click();
    flushTasks();
    assertTrue(!navigationElement.changeCalled);
  });

  test('AskOnStartupSingleToMultipleProfiles', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$$('#wrapper').hidden);
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
    let profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    // The checkbox 'Ask when chrome opens' should only be visible to
    // multi-profile users.
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
    // Add a second profile.
    profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$$('cr-checkbox').hidden);
    assertTrue(mainViewElement.$$('cr-checkbox').checked);
    mainViewElement.$$('cr-checkbox').click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$$('cr-checkbox').checked);
  });

  test('AskOnStartupMultipleToSingleProfile', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$$('#wrapper').hidden);
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$$('cr-checkbox').hidden);
    // Remove profile.
    webUIListenerCallback('profile-removed', profiles[0].profilePath);
    flushTasks();
    await verifyProfileCard(
        [profiles[1]],
        mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
  });

  test('AskOnStartupMulipleProfiles', async function() {
    // Disable AskOnStartup
    loadTimeData.overrideValues({disableAskOnStartup: true});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$$('#wrapper').hidden);
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));

    // Checkbox hidden even if there are multiple profiles because of
    // disableAskOnStartup.
    assertTrue(mainViewElement.$$('cr-checkbox').hidden);
  });

  test('ForceSigninIsEnabled', async function() {
    loadTimeData.overrideValues({isForceSigninEnabled: true});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(2);
    profiles[0].needsSignin = true;
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
  });
});
