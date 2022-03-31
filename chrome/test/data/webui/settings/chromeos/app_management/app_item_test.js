// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {FakePageHandler} from 'chrome://os-settings/chromeos/os_settings.js';
import {replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://test/test_util.js';
import {Router} from 'chrome://os-settings/chromeos/os_settings.js';

suite('<app-management-app-item>', () => {
  let appItem;
  let arcApp;

  setup(async () => {
    appItem = document.createElement('app-management-app-item');
    replaceBody(appItem);
    await flushTasks();

    // Create an ARC app.
    const arcOptions = {type: appManagement.mojom.AppType.kArc};

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
    const icon = appItem.$$('#app-icon');

    assertTrue(!!icon);
    assertEquals(appItem.iconUrlFromId_(arcApp), icon.getAttribute('src'));
  });
});
