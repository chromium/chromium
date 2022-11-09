// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';

import {loadTimeData, ManageProfilesBrowserProxyImpl, NavigationMixin, ProfileCardElement, ProfilePickerMainViewElement, ProfileState, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

class NavigationElement extends NavigationMixin
(PolymerElement) {
  static get is() {
    return 'navigation-element';
  }

  changeCalled: boolean = false;
  route: string = '';

  override ready() {
    super.ready();
    this.reset();
  }

  override onRouteChange(route: Routes, _step: string) {
    this.changeCalled = true;
    this.route = route;
  }

  reset() {
    this.changeCalled = false;
    this.route = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'navigation-element': NavigationElement;
  }
}

customElements.define(NavigationElement.is, NavigationElement);

suite('ProfilePickerMainViewTest', function() {
  let mainViewElement: ProfilePickerMainViewElement;
  let browserProxy: TestManageProfilesBrowserProxy;
  let navigationElement: NavigationElement;

  function resetTest() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigationElement = document.createElement('navigation-element');
    document.body.appendChild(navigationElement);
    mainViewElement = document.createElement('profile-picker-main-view');
    document.body.appendChild(mainViewElement);
    return waitBeforeNextRender(mainViewElement);
  }

  function resetPolicies() {
    // This is necessary as |loadTimeData| state leaks between tests.
    // Any load time data manipulated by the tests needs to be reset here.
    loadTimeData.overrideValues({
      isGuestModeEnabled: true,
      isProfileCreationAllowed: true,
      isAskOnStartupAllowed: true,
    });
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    resetPolicies();
    return resetTest();
  });

  /**
   * @param n Indicates the desired number of profiles.
   */
  function generateProfilesList(n: number): ProfileState[] {
    return Array(n)
        .fill(0)
        .map((_x, i) => i % 2 === 0)
        .map((sync, i) => ({
               profilePath: `profilePath${i}`,
               localProfileName: `profile${i}`,
               isSyncing: sync,
               needsSignin: false,
               gaiaName: sync ? `User${i}` : '',
               userName: sync ? `User${i}@gmail.com` : '',
               isManaged: i % 4 === 0,
               avatarIcon: `AvatarUrl-${i}`,
               // <if expr="chromeos_lacros">
               isPrimaryLacrosProfile: false,
               // </if>
             }));
  }

  async function verifyProfileCard(
      expectedProfiles: ProfileState[],
      profiles: NodeListOf<ProfileCardElement>) {
    assertEquals(expectedProfiles.length, profiles.length);
    for (let i = 0; i < expectedProfiles.length; i++) {
      const profile = profiles[i]!;
      const expectedProfile = expectedProfiles[i]!;
      assertTrue(!!profile.shadowRoot!.querySelector('profile-card-menu'));
      profile.shadowRoot!.querySelector('cr-button')!.click();
      await browserProxy.whenCalled('launchSelectedProfile');
      assertEquals(
          profile.shadowRoot!
              .querySelector<HTMLElement>('#forceSigninContainer')!.hidden,
          !expectedProfile.needsSignin);

      const gaiaName = profile.$.gaiaName;
      assertEquals(gaiaName.hidden, expectedProfile.needsSignin);
      assertEquals(gaiaName.innerText.trim(), expectedProfile.gaiaName);

      assertEquals(profile.$.nameInput.value, expectedProfile.localProfileName);
      assertEquals(
          profile.shadowRoot!.querySelector<HTMLElement>(
                                 '#iconContainer')!.hidden,
          !expectedProfile.isManaged);
      assertEquals(
          (profile.shadowRoot!
               .querySelector<HTMLImageElement>('.profile-avatar')!.src)
              .split('/')
              .pop(),
          expectedProfile.avatarIcon);
    }
  }

  test('MainViewWithDefaultPolicies', async function() {
    assertTrue(navigationElement.changeCalled);
    assertEquals(navigationElement.route, Routes.MAIN);
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(6);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    // Profiles list defined.
    assertTrue(!mainViewElement.$.profilesContainer.hidden);
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    assertTrue(mainViewElement.$.askOnStartup.checked);
    // Verify profile card.
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // Browse as guest.
    assertTrue(!!mainViewElement.$.browseAsGuestButton);
    mainViewElement.$.browseAsGuestButton.click();
    await browserProxy.whenCalled('launchGuestProfile');
    // Ask when chrome opens.
    mainViewElement.$.askOnStartup.click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$.askOnStartup.checked);
    // Update profile data.
    profiles[1] = profiles[4]!;
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // Profiles update on remove.
    webUIListenerCallback('profile-removed', profiles[3]!.profilePath);
    profiles.splice(3, 1);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
  });

  test('EditLocalProfileName', async function() {
    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    const localProfileName =
        mainViewElement.shadowRoot!.querySelector('profile-card')!.$.nameInput;
    assertEquals(localProfileName.value, profiles[0]!.localProfileName);

    // Set to valid profile name.
    localProfileName.value = 'Alice';
    localProfileName.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const args = await browserProxy.whenCalled('setProfileName');
    assertEquals(args[0], profiles[0]!.profilePath);
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
    assertEquals(mainViewElement.$.browseAsGuestButton.style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    assertEquals(mainViewElement.$.browseAsGuestButton.style.display, 'none');
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    resetTest();
    const addProfile =
        mainViewElement.shadowRoot!.querySelector<HTMLElement>('#addProfile')!;
    assertEquals(addProfile.style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    navigationElement.reset();
    assertEquals(addProfile.style.display, 'none');
    addProfile.click();
    flushTasks();
    assertTrue(!navigationElement.changeCalled);
  });

  test('AskOnStartupSingleToMultipleProfiles', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    let profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // The checkbox 'Ask when chrome opens' should only be visible to
    // multi-profile users.
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    // Add a second profile.
    profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    assertTrue(mainViewElement.$.askOnStartup.checked);
    mainViewElement.$.askOnStartup.click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$.askOnStartup.checked);
  });

  test('AskOnStartupMultipleToSingleProfile', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    // Remove profile.
    webUIListenerCallback('profile-removed', profiles[0]!.profilePath);
    flushTasks();
    await verifyProfileCard(
        [profiles[1]!],
        mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(mainViewElement.$.askOnStartup.hidden);
  });

  test('AskOnStartupMulipleProfiles', async function() {
    // Disable AskOnStartup
    loadTimeData.overrideValues({isAskOnStartupAllowed: false});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));

    // Checkbox hidden even if there are multiple profiles.
    assertTrue(mainViewElement.$.askOnStartup.hidden);
  });

  test('ForceSigninIsEnabled', async function() {
    loadTimeData.overrideValues({isForceSigninEnabled: true});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(2);
    profiles[0]!.needsSignin = true;
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
  });
});
