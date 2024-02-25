// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import type {ProfileTypeChoiceElement} from 'chrome://profile-picker/lazy_load.js';
import {ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfileTypeChoiceTest', function() {
  let choice: ProfileTypeChoiceElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    assertTrue(isChildVisible(choice, '#notNowButton'));
  });

  test('VerifySignInPromoImpressionRecorded', function() {
    return browserProxy.whenCalled('recordSignInPromoImpression');
  });
});
