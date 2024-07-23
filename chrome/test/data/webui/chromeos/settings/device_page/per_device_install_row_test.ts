// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {PerDeviceInstallRowElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

suite(PerDeviceInstallRowElement.is, () => {
  let installRow: PerDeviceInstallRowElement;

  async function createInstallRow() {
    clearBody();
    installRow = document.createElement(PerDeviceInstallRowElement.is);
    document.body.appendChild(installRow);
    return flushTasks();
  }

  test('Initialize per-device-install-row', async () => {
    await createInstallRow();
    assertTrue(!!installRow.shadowRoot!.querySelector('.app-info-container'));
  });
});
