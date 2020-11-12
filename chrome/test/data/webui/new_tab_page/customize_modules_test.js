// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {$$, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageCustomizeModulesTest', () => {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;
  });

  [true, false].forEach(visible => {
    test('toggle hide calls handler', async () => {
      // Arrange.
      const customizeModules = document.createElement('ntp-customize-modules');
      document.body.appendChild(customizeModules);
      testProxy.callbackRouterRemote.setModulesVisible(visible);

      // Act.
      customizeModules.$.hideToggle.click();
      customizeModules.apply();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('setModulesVisible'));
      assertStyle(
          $$(customizeModules, 'cr-policy-indicator'), 'display', 'none');
    });
  });

  test('policy disables hide toggle', () => {
    // Act.
    loadTimeData.overrideValues({modulesVisibleManagedByPolicy: true});
    const customizeModules = document.createElement('ntp-customize-modules');
    document.body.appendChild(customizeModules);

    // Assert.
    assertTrue(customizeModules.$.hideToggle.disabled);
    assertNotStyle(
        $$(customizeModules, 'cr-policy-indicator'), 'display', 'none');
  });
});
