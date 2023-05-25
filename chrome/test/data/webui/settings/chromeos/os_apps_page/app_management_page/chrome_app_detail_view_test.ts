// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementChromeAppDetailViewElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-chrome-app-detail-view>', () => {
  let chromeAppDetailView: AppManagementChromeAppDetailViewElement;
  let fakeHandler: FakePageHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const chromeOptions = {
      type: AppType.kExtension,
      permissions: {},
    };

    // Add an chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp('', chromeOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    fakeHandler.flushPipesForTesting();
    await flushTasks();

    chromeAppDetailView =
        document.createElement('app-management-chrome-app-detail-view');
    replaceBody(chromeAppDetailView);
    fakeHandler.flushPipesForTesting();
    await flushTasks();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        chromeAppDetailView.get('app_').id);
    assertTrue(!!chromeAppDetailView.shadowRoot!.querySelector(
        'app-management-pin-to-shelf-item'));
    assertTrue(
        !!chromeAppDetailView.shadowRoot!.querySelector('#moreSettings'));
  });
});
