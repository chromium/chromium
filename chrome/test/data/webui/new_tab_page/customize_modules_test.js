// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {$$, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

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
    test('toggle show calls handler', async () => {
      // Arrange.
      const customizeModules = document.createElement('ntp-customize-modules');
      document.body.appendChild(customizeModules);
      testProxy.callbackRouterRemote.setModulesVisible(visible);

      // setModulesVisible sets visible to true, but customizeModules' show_
      // isn't immediately updated after. Using flushForTesting ensures
      // everything from the callback is done updating before proceeding.
      await testProxy.callbackRouterRemote.$.flushForTesting();

      // Act.
      customizeModules.$.showToggle.click();
      customizeModules.apply();

      // Assert.
      assertEquals(
          !visible, await testProxy.handler.whenCalled('setModulesVisible'));
      assertStyle(
          $$(customizeModules, 'cr-policy-indicator'), 'display', 'none');
    });
  });

  test('policy disables show toggle', () => {
    // Act.
    loadTimeData.overrideValues({modulesVisibleManagedByPolicy: true});
    const customizeModules = document.createElement('ntp-customize-modules');
    document.body.appendChild(customizeModules);

    // Assert.
    assertTrue(customizeModules.$.showToggle.disabled);
    assertNotStyle(
        $$(customizeModules, 'cr-policy-indicator'), 'display', 'none');
  });
});
