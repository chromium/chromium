// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody} from './test_util.js';
import {Router} from 'chrome://os-settings/chromeos/os_settings.js';

suite('<app-management-main-view>', function() {
  let mainView;
  let fakeHandler;
  let store;

  /**
   * @param {number} numApps
   */
  async function addApps(numApps) {
    for (let i = 0; i < numApps; i++) {
      await fakeHandler.addApp();
    }
  }

  function getAppItems() {
    return mainView.shadowRoot.querySelector('#appList')
        .querySelectorAll('app-management-app-item');
  }

  setup(function() {
    fakeHandler = setupFakeHandler();
    store = replaceStore();

    mainView = document.createElement('app-management-main-view');
    replaceBody(mainView);
  });

  test('simple app addition', async function() {
    // Ensure there is no apps initially
    assertEquals(0, getAppItems().length);

    const app = await fakeHandler.addApp();

    const appItems = getAppItems();
    assertEquals(1, appItems.length);
    assertFalse(!!Router.getInstance().getQueryParameters().get('id'));
    store.setReducersEnabled(false);

    appItems[0].click();
    fakeHandler.flushPipesForTesting();
    assertTrue(!!Router.getInstance().getQueryParameters().get('id'));
  });
});
