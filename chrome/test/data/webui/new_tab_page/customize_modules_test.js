// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageCustomizeModulesTest', () => {
  /** @type {!CustomizeModulesElement} */
  let customizeModules;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;

    customizeModules = document.createElement('ntp-customize-modules');
    document.body.appendChild(customizeModules);
  });

  [true, false].forEach(visible => {
    test('toggle hide calls handler', async () => {
      // Arrange.
      testProxy.callbackRouterRemote.setModulesVisible(visible);

      // Act.
      customizeModules.$.hideToggle.click();
      customizeModules.apply();

      // Assert.
      assertEquals(1, testProxy.handler.getCallCount('setModulesVisible'));
    });
  });
});
