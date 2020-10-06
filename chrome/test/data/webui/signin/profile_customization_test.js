// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-customization/profile_customization_app.js';

import {ProfileCustomizationBrowserProxyImpl} from 'chrome://profile-customization/profile_customization_browser_proxy.js';

import {assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

import {TestProfileCustomizationBrowserProxy} from './test_profile_customization_browser_proxy.js';


suite('ProfileCustomizationTest', function() {
  /** @type {!ProfileCustomizationAppElement} */
  let app;

  /** @type {!TestProfileCustomizationBrowserProxy} */
  let browserProxy;

  setup(function() {
    browserProxy = new TestProfileCustomizationBrowserProxy();
    ProfileCustomizationBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    app = /** @type {!ProfileCustomizationAppElement} */ (
        document.createElement('profile-customization-app'));
    document.body.append(app);
  });

  test('ClickDone', function() {
    assertTrue(isChildVisible(app, '#doneButton'));
    app.$$('#doneButton').click();
    return browserProxy.whenCalled('done');
  });

  test('ThemeSelector', function() {
    assertTrue(!!app.$$('#themeSelector'));
  });
});
