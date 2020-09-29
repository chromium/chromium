// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, Routes} from 'chrome://profile-picker/profile_picker.js';

import {assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfilePickerAppTest', function() {
  /** @type {!ProfilePickerAppElement} */
  let app;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.instance_ = browserProxy;

    // Reset state of the test element.
    document.body.innerHTML = '';
    navigateTo(Routes.MAIN);
    app = /** @type {!ProfilePickerAppElement} */ (
        document.createElement('profile-picker-app'));
    document.body.appendChild(app);
  });

  /**
   * @return {!Promise} Promise that resolves when initialization is complete
   *     and the lazy loaded module has been loaded.
   */
  function waitForLoad() {
    return Promise.all([
      browserProxy.whenCalled('getNewProfileSuggestedThemeInfo'),
      ensureLazyLoaded(),
    ]);
  }

  test('signInButtonImplementation', function() {
    navigateTo(Routes.NEW_PROFILE);
    return waitForLoad()
        .then(() => {
          return waitBeforeNextRender(app);
        })
        .then(() => {
          const choice = /** @type {!ProfileTypeChoiceElement} */ (
              app.$$('profile-type-choice'));
          assertTrue(!!choice);
          choice.$$('#signInButton').click();
          assertTrue(choice.$$('#signInButton').disabled);
          assertTrue(choice.$$('#notNowButton').disabled);
          return browserProxy.whenCalled('loadSignInProfileCreationFlow');
        });
  });

  test('notNowButtonImplementation', function() {
    navigateTo(Routes.NEW_PROFILE);
    return waitForLoad()
        .then(() => {
          return waitBeforeNextRender(app);
        })
        .then(() => {
          const choice = /** @type {!ProfileTypeChoiceElement} */ (
              app.$$('profile-type-choice'));
          assertTrue(!!choice);
          choice.$$('#notNowButton').click();
          const customization =
              /** @type {!LocalProfileCustomizationElement} */ (
                  app.$$('local-profile-customization'));
          assertTrue(!!customization);
          assertTrue(customization.classList.contains('active'));
        });
  });
});
