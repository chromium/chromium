// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {PerDeviceAppInstalledRowElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

suite(PerDeviceAppInstalledRowElement.is, () => {
  let appRow: PerDeviceAppInstalledRowElement;

  async function createAppRow() {
    clearBody();
    appRow = document.createElement(PerDeviceAppInstalledRowElement.is);
    document.body.appendChild(appRow);
    return flushTasks();
  }

  test('Initialize per-device-app-installed-row', async () => {
    await createAppRow();
    assertTrue(!!appRow.shadowRoot!.querySelector('.app-installed-row'));
  });
});
