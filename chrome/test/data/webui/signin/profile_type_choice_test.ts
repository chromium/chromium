// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import {ProfileTypeChoiceElement} from 'chrome://profile-picker/lazy_load.js';
import {ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {isLacros} from 'chrome://resources/js/cr.m.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfileTypeChoiceTest', function() {
  let choice: ProfileTypeChoiceElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    choice = document.createElement('profile-type-choice');
    document.body.append(choice);
  });

  test('BackButton', function() {
    assertTrue(isChildVisible(choice, '#backButton'));
  });

  test('SignInButton', function() {
    assertTrue(isChildVisible(choice, '#signInButton'));
  });

  test('NotNowButton', function() {
    // Local profile creation is not enabled on Lacros.
    assertEquals(isChildVisible(choice, '#notNowButton'), !isLacros);
  });

  test('VerifySignInPromoImpressionRecorded', function() {
    return browserProxy.whenCalled('recordSignInPromoImpression');
  });
});
