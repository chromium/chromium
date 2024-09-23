// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import type {ProfileSwitchElement} from 'chrome://profile-picker/lazy_load.js';
import type {ProfileState} from 'chrome://profile-picker/profile_picker.js';
import {ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfileSwitchTest', function() {
  let profileSwitchElement: ProfileSwitchElement;
  let browserProxy: TestManageProfilesBrowserProxy;
  let getSwitchProfilePromiseResolver: PromiseResolver<ProfileState>;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    getSwitchProfilePromiseResolver = new PromiseResolver();
    browserProxy.setGetSwitchProfilePromise(
        getSwitchProfilePromiseResolver.promise);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    profileSwitchElement = document.createElement('profile-switch');
    document.body.appendChild(profileSwitchElement);
  });

  test('getSwitchProfile', async function() {
    assertTrue(profileSwitchElement.$.switchButton.disabled);

    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');
    await microtasksFinished();

    assertFalse(profileSwitchElement.$.switchButton.disabled);
    assertEquals(
        profileSwitchElement.shadowRoot!
            .querySelector<HTMLImageElement>('img.profile-avatar')!.src
            .split('/')
            .pop(),
        'url');
    assertEquals(profileSwitchElement.$.profileName.innerText, 'Work');
    assertEquals(profileSwitchElement.$.gaiaName.innerText, 'Alice');
    assertTrue(profileSwitchElement.$.iconContainer.hidden);
  });

  test('getSwitchProfile_managed', async function() {
    const profileState: ProfileState =
        Object.assign({}, browserProxy.profileSample);
    profileState.avatarBadge = 'cr:domain';

    getSwitchProfilePromiseResolver.resolve(profileState);
    await browserProxy.whenCalled('getSwitchProfile');
    await microtasksFinished();

    assertFalse(profileSwitchElement.$.iconContainer.hidden);
  });

  test('confirmSwitch', async function() {
    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');
    await microtasksFinished();

    assertFalse(profileSwitchElement.$.switchButton.disabled);
    profileSwitchElement.$.switchButton.click();
    const [profilePath] = await browserProxy.whenCalled('confirmProfileSwitch');
    assertEquals(profilePath, 'profile1');
  });

  test('cancelSwitch_beforeGetSwitchProfile', async function() {
    assertFalse(profileSwitchElement.$.cancelButton.disabled);
    profileSwitchElement.$.cancelButton.click();
    await browserProxy.whenCalled('cancelProfileSwitch');
  });

  test('cancelSwitch_afterGetSwitchProfile', async function() {
    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');
    await microtasksFinished();

    assertFalse(profileSwitchElement.$.cancelButton.disabled);
    profileSwitchElement.$.cancelButton.click();
    await browserProxy.whenCalled('cancelProfileSwitch');
  });
});
