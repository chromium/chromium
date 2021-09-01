// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProfileSwitchElement} from 'chrome://profile-picker/lazy_load.js';

import {ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfileSwitchTest', function() {
  /** @type {!ProfileSwitchElement} */
  let profileSwitchElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  /** @type {!PromiseResolver} */
  let getSwitchProfilePromiseResolver;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    getSwitchProfilePromiseResolver = new PromiseResolver();
    browserProxy.setGetSwitchProfilePromise(
        getSwitchProfilePromiseResolver.promise);
    document.body.innerHTML = '';
    profileSwitchElement = /** @type {!ProfileSwitchElement} */ (
        document.createElement('profile-switch'));
    document.body.appendChild(profileSwitchElement);
    return waitBeforeNextRender(profileSwitchElement);
  });

  test('getSwitchProfile', async function() {
    assertTrue(profileSwitchElement.shadowRoot.querySelector('#switchButton')
                   .disabled);

    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(profileSwitchElement.shadowRoot.querySelector('#switchButton')
                    .disabled);
    assertEquals(
        profileSwitchElement.shadowRoot.querySelector('img.profile-avatar')
            .src.split('/')
            .pop(),
        'url');
    assertEquals(
        profileSwitchElement.shadowRoot.querySelector('#profileName').innerText,
        'Work');
    assertEquals(
        profileSwitchElement.shadowRoot.querySelector('#gaiaName').innerText,
        'Alice');
    assertTrue(
        profileSwitchElement.shadowRoot.querySelector('#iconContainer').hidden);
  });

  test('getSwitchProfile_managed', async function() {
    const profileState = /** @type {!ProfileState} */ (
        Object.assign({}, browserProxy.profileSample));
    profileState.isManaged = true;

    getSwitchProfilePromiseResolver.resolve(profileState);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(
        profileSwitchElement.shadowRoot.querySelector('#iconContainer').hidden);
  });

  test('confirmSwitch', async function() {
    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(profileSwitchElement.shadowRoot.querySelector('#switchButton')
                    .disabled);
    profileSwitchElement.shadowRoot.querySelector('#switchButton').click();
    const [profilePath] = await browserProxy.whenCalled('confirmProfileSwitch');
    assertEquals(profilePath, 'profile1');
  });

  test('cancelSwitch_beforeGetSwitchProfile', async function() {
    assertFalse(profileSwitchElement.shadowRoot.querySelector('#cancelButton')
                    .disabled);
    profileSwitchElement.shadowRoot.querySelector('#cancelButton').click();
    await browserProxy.whenCalled('cancelProfileSwitch');
  });

  test('cancelSwitch_afterGetSwitchProfile', async function() {
    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(profileSwitchElement.shadowRoot.querySelector('#cancelButton')
                    .disabled);
    profileSwitchElement.shadowRoot.querySelector('#cancelButton').click();
    await browserProxy.whenCalled('cancelProfileSwitch');
  });
});
