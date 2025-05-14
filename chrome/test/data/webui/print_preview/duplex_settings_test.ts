// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDuplexSettingsElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {DuplexMode} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('DuplexSettingsTest', function() {
  let duplexSection: PrintPreviewDuplexSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.setSettingAvailableForTesting('duplex', true);
    model.setSetting('duplex', false, /*noSticky=*/ true);
    model.setSettingAvailableForTesting('duplexShortEdge', true);

    duplexSection = document.createElement('print-preview-duplex-settings');
    duplexSection.disabled = false;
    document.body.appendChild(duplexSection);
  });

  // Tests that making short edge unavailable prevents the collapse from
  // showing.
  test('short edge unavailable', async function() {
    const collapse = duplexSection.shadowRoot.querySelector('cr-collapse')!;
    duplexSection.setSetting('duplex', true);
    await microtasksFinished();
    assertTrue(collapse.opened);

    for (const value of [false, true]) {
      model.setSettingAvailableForTesting('duplexShortEdge', value);
      await microtasksFinished();
      assertEquals(value, collapse.opened);
    }
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const checkbox = duplexSection.shadowRoot.querySelector('cr-checkbox')!;
    const collapse = duplexSection.shadowRoot.querySelector('cr-collapse')!;
    assertFalse(checkbox.checked);
    assertFalse(collapse.opened);

    duplexSection.setSetting('duplex', true);
    await microtasksFinished();
    assertTrue(checkbox.checked);
    assertTrue(collapse.opened);

    const select = duplexSection.shadowRoot.querySelector('select')!;
    assertEquals(DuplexMode.LONG_EDGE.toString(), select.value);

    duplexSection.setSetting('duplexShortEdge', true);
    await microtasksFinished();
    assertEquals(DuplexMode.SHORT_EDGE.toString(), select.value);
  });

  // Tests that checking the box or selecting a new option in the dropdown
  // updates the setting.
  test('select option', async () => {
    const checkbox = duplexSection.shadowRoot.querySelector('cr-checkbox')!;
    const collapse = duplexSection.shadowRoot.querySelector('cr-collapse')!;
    assertFalse(checkbox.checked);
    assertFalse(collapse.opened);
    assertFalse(duplexSection.getSettingValue('duplex') as boolean);
    assertFalse(duplexSection.getSettingValue('duplexShortEdge') as boolean);
    assertFalse(duplexSection.getSetting('duplex').setFromUi);
    assertFalse(duplexSection.getSetting('duplexShortEdge').setFromUi);

    checkbox.checked = true;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertTrue(collapse.opened);
    assertTrue(duplexSection.getSettingValue('duplex') as boolean);
    assertFalse(duplexSection.getSettingValue('duplexShortEdge') as boolean);
    assertTrue(duplexSection.getSetting('duplex').setFromUi);
    assertFalse(duplexSection.getSetting('duplexShortEdge').setFromUi);

    const select = duplexSection.shadowRoot.querySelector('select')!;
    assertEquals(DuplexMode.LONG_EDGE.toString(), select.value);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(duplexSection, DuplexMode.SHORT_EDGE.toString());
    assertTrue(duplexSection.getSettingValue('duplex') as boolean);
    assertTrue(duplexSection.getSettingValue('duplexShortEdge') as boolean);
    assertTrue(duplexSection.getSetting('duplex').setFromUi);
    assertTrue(duplexSection.getSetting('duplexShortEdge').setFromUi);
  });
});
