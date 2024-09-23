// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementAppItemElement} from 'chrome://os-settings/lazy_load.js';
import {Router} from 'chrome://os-settings/os_settings.js';
import {App, AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {replaceBody} from '../../app_management/test_util.js';

suite('<app-management-app-item>', () => {
  let appItem: AppManagementAppItemElement;
  let arcApp: App;

  setup(async () => {
    appItem = document.createElement('app-management-app-item');
    replaceBody(appItem);
    await flushTasks();

    // Create an ARC app.
    const arcOptions = {type: AppType.kArc};

    arcApp = FakePageHandler.createApp('app1_id', arcOptions);
    appItem.app = arcApp;

    await flushTasks();
  });

  teardown(() => {
    appItem.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Onclick redirects to correct route', async () => {
    assertNull(Router.getInstance().getQueryParameters().get('id'));

    appItem.click();
    await flushTasks();
    assertEquals(
        arcApp.id, Router.getInstance().getQueryParameters().get('id'));
  });

  test('Icon renders', () => {
    const icon = appItem.shadowRoot!.querySelector('#appIcon');

    assertTrue(!!icon);
    assertEquals(appItem['iconUrlFromId_'](arcApp), icon.getAttribute('src'));
  });
});
