// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {PerDeviceSubsectionHeaderElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

suite(PerDeviceSubsectionHeaderElement.is, () => {
  let subsectionHeader: PerDeviceSubsectionHeaderElement;

  async function createHeaderElement() {
    clearBody();
    subsectionHeader =
        document.createElement(PerDeviceSubsectionHeaderElement.is);
    document.body.appendChild(subsectionHeader);
    return flushTasks();
  }

  test('Initialize per-device-subsection-header', async () => {
    await createHeaderElement();
    assertTrue(!!subsectionHeader.shadowRoot!.querySelector(
        '#perDeviceSubsectionHeader'));
  });
});
