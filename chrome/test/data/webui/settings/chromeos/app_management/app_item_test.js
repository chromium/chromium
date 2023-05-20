// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {FakePageHandler} from './fake_page_handler.js';
import {replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {Router} from 'chrome://os-settings/os_settings.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-app-item>', () => {
  let appItem;
  let arcApp;

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

  test('Onclick redirects to correct route', async () => {
    assertFalse(!!Router.getInstance().getQueryParameters().get('id'));

    appItem.click();
    await flushTasks();
    assertEquals(
        arcApp.id, Router.getInstance().getQueryParameters().get('id'));
  });

  test('Icon renders', async () => {
    const icon = appItem.shadowRoot.querySelector('#appIcon');

    assertTrue(!!icon);
    assertEquals(appItem.iconUrlFromId_(arcApp), icon.getAttribute('src'));
  });
});
