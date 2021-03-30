// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import {ManageProfilesBrowserProxyImpl, ProfileState} from 'chrome://profile-picker/profile_picker.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfilePickerMainViewTest', function() {
  /** @type {!ProfileSwitchElement} */
  let profileSwitchElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  /** @type {!PromiseResolver} */
  let getSwitchProfilePromiseResolver;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.instance_ = browserProxy;
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
    assertTrue(profileSwitchElement.$$('#switchButton').disabled);

    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(profileSwitchElement.$$('#switchButton').disabled);
    assertEquals(
        profileSwitchElement.$$('img.profile-avatar').src.split('/').pop(),
        'url');
    assertEquals(profileSwitchElement.$$('#profileName').innerText, 'Work');
    assertEquals(profileSwitchElement.$$('#gaiaName').innerText, 'Alice');
  });

  test('confirmSwitch', async function() {
    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(profileSwitchElement.$$('#switchButton').disabled);
    profileSwitchElement.$$('#switchButton').click();
    const [profilePath] = await browserProxy.whenCalled('confirmProfileSwitch');
    assertEquals(profilePath, 'profile1');
  });

  test('cancelSwitch_beforeGetSwitchProfile', async function() {
    assertFalse(profileSwitchElement.$$('#cancelButton').disabled);
    profileSwitchElement.$$('#cancelButton').click();
    await browserProxy.whenCalled('cancelProfileSwitch');
  });

  test('cancelSwitch_afterGetSwitchProfile', async function() {
    getSwitchProfilePromiseResolver.resolve(browserProxy.profileSample);
    await browserProxy.whenCalled('getSwitchProfile');

    assertFalse(profileSwitchElement.$$('#cancelButton').disabled);
    profileSwitchElement.$$('#cancelButton').click();
    await browserProxy.whenCalled('cancelProfileSwitch');
  });
});
