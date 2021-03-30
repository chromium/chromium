// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakePageHandler} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// #import {Router} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

'use strict';

suite('<app-management-app-item>', () => {
  let appItem;
  let arcApp;

  setup(async () => {
    appItem = document.createElement('app-management-app-item');
    replaceBody(appItem);
    await test_util.flushTasks();

    // Create an ARC app.
    const arcOptions = {type: apps.mojom.AppType.kArc};

    arcApp = app_management.FakePageHandler.createApp('app1_id', arcOptions);
    appItem.app = arcApp;

    await test_util.flushTasks();
  });

  test('Onclick redirects to correct route', async () => {
    assertFalse(!!settings.Router.getInstance().getQueryParameters().get('id'));

    appItem.click();
    await test_util.flushTasks();
    assertEquals(
        arcApp.id,
        settings.Router.getInstance().getQueryParameters().get('id'));
  });

  test('Icon renders', async () => {
    const icon = appItem.$$('#app-icon');

    assertTrue(!!icon);
    assertEquals(appItem.iconUrlFromId_(arcApp), icon.getAttribute('src'));
  });
});
