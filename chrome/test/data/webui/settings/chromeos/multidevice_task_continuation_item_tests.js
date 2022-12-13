// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SyncBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSyncBrowserProxy} from './test_os_sync_browser_proxy.js';

function getPrefs() {
  return {
    tabsSynced: true,
  };
}

suite('Multidevice', function() {
  let taskContinuationItem;

  setup(function() {
    const browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    PolymerTest.clearBody();

    taskContinuationItem =
        document.createElement('settings-multidevice-task-continuation-item');
    document.body.appendChild(taskContinuationItem);

    flush();
  });

  teardown(function() {
    taskContinuationItem.remove();
  });

  test('Chrome Sync off', async () => {
    const prefs = getPrefs();
    prefs.tabsSynced = false;
    flush();

    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertTrue(!!taskContinuationItem.shadowRoot.querySelector(
        'settings-multidevice-task-continuation-disabled-link'));

    const toggle = taskContinuationItem.shadowRoot.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
  });

  test('Chrome Sync on', async () => {
    const prefs = getPrefs();
    prefs.tabsSynced = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertFalse(!!taskContinuationItem.shadowRoot.querySelector(
        'settings-multidevice-task-continuation-disabled-link'));
  });
});
