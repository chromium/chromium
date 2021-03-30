// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {PageType} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody} from './test_util.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

'use strict';

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
    return mainView.$$('#app-list').querySelectorAll('app-management-app-item');
  }

  setup(function() {
    fakeHandler = setupFakeHandler();
    store = replaceStore();

    mainView = document.createElement('app-management-main-view');
    replaceBody(mainView);
  });

  test('simple app addition', async function() {
    // Ensure there is no apps initially
    expectEquals(0, getAppItems().length);

    const app = await fakeHandler.addApp();

    const appItems = getAppItems();
    expectEquals(1, appItems.length);
    assertFalse(!!settings.Router.getInstance().getQueryParameters().get('id'));
    store.setReducersEnabled(false);

    appItems[0].click();
    fakeHandler.flushPipesForTesting();
    assertTrue(!!settings.Router.getInstance().getQueryParameters().get('id'));
  });
});
