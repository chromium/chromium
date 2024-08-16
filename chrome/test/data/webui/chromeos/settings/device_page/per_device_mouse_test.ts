// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeMice, fakeMice2, PerDeviceSubsectionHeaderElement, SettingsPerDeviceMouseElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-per-device-mouse>', function() {
  let perDeviceMousePage: SettingsPerDeviceMouseElement;

  setup(async function initializePerDeviceMousePage() {
    perDeviceMousePage = document.createElement('settings-per-device-mouse');
    assert(perDeviceMousePage);
    perDeviceMousePage.set('mice', fakeMice);
    document.body.appendChild(perDeviceMousePage);
    await flushTasks();
  });

  teardown(() => {
    perDeviceMousePage.remove();
  });

  test('Mouse page updates with new mice', async () => {
    let subsections = perDeviceMousePage.shadowRoot!.querySelectorAll(
        'settings-per-device-mouse-subsection');
    assertEquals(fakeMice.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const subsectionHeader = strictQuery(
          'per-device-subsection-header', subsections[i]!.shadowRoot,
          PerDeviceSubsectionHeaderElement);
      const name =
          subsectionHeader.shadowRoot!.querySelector('h2')!.textContent!.trim();
      assertEquals(fakeMice[i]!.name, name);
    }
    assertFalse(subsections[0]!.get('isLastDevice'));
    assertTrue(subsections[fakeMice.length - 1]!.get('isLastDevice'));

    // Check the number and name of subsections when the mouse list is updated.
    perDeviceMousePage.set('mice', fakeMice2);
    await flushTasks();
    subsections = perDeviceMousePage.shadowRoot!.querySelectorAll(
        'settings-per-device-mouse-subsection');
    assertEquals(fakeMice2.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const subsectionHeader = strictQuery(
          'per-device-subsection-header', subsections[i]!.shadowRoot,
          PerDeviceSubsectionHeaderElement);
      const name =
          subsectionHeader.shadowRoot!.querySelector('h2')!.textContent!.trim();
      assertEquals(fakeMice2[i]!.name, name);
    }
    assertTrue(subsections[fakeMice2.length - 1]!.get('isLastDevice'));
  });
});
