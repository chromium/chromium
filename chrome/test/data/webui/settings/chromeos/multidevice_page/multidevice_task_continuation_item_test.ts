// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsMultideviceTaskContinuationItemElement} from 'chrome://os-settings/lazy_load.js';
import {SyncBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSyncBrowserProxy} from '../test_os_sync_browser_proxy.js';

function getPrefs() {
  return {
    tabsSynced: true,
  };
}

suite('<settings-multidevice-task-continuation-item>', () => {
  let taskContinuationItem: SettingsMultideviceTaskContinuationItemElement;

  setup(() => {
    const browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    taskContinuationItem =
        document.createElement('settings-multidevice-task-continuation-item');
    document.body.appendChild(taskContinuationItem);

    flush();
  });

  teardown(() => {
    taskContinuationItem.remove();
  });

  test('Chrome Sync off', async () => {
    const prefs = getPrefs();
    prefs.tabsSynced = false;
    flush();

    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertTrue(!!taskContinuationItem.shadowRoot!.querySelector(
        'settings-multidevice-task-continuation-disabled-link'));

    const toggle = taskContinuationItem.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
  });

  test('Chrome Sync on', async () => {
    const prefs = getPrefs();
    prefs.tabsSynced = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertEquals(
        null,
        taskContinuationItem.shadowRoot!.querySelector(
            'settings-multidevice-task-continuation-disabled-link'));
  });
});
