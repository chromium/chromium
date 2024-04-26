// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDuplexSettingsElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {DuplexMode} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('DuplexSettingsTest', function() {
  let duplexSection: PrintPreviewDuplexSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.duplex.available', true);
    model.set('settings.duplex.value', false);
    model.set('settings.duplexShortEdge.available', true);

    duplexSection = document.createElement('print-preview-duplex-settings');
    duplexSection.settings = model.settings;
    duplexSection.disabled = false;
    fakeDataBind(model, duplexSection, 'settings');
    document.body.appendChild(duplexSection);
    flush();
  });

  // Tests that making short edge unavailable prevents the collapse from
  // showing.
  test('short edge unavailable', function() {
    const collapse = duplexSection.shadowRoot!.querySelector('cr-collapse')!;
    duplexSection.setSetting('duplex', true);
    assertTrue(collapse.opened);

    [false, true].forEach(value => {
      model.set('settings.duplexShortEdge.available', value);
      assertEquals(value, collapse.opened);
    });
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const checkbox = duplexSection.shadowRoot!.querySelector('cr-checkbox')!;
    const collapse = duplexSection.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(checkbox.checked);
    assertFalse(collapse.opened);

    duplexSection.setSetting('duplex', true);
    assertTrue(checkbox.checked);
    assertTrue(collapse.opened);

    const select = duplexSection.shadowRoot!.querySelector('select')!;
    assertEquals(DuplexMode.LONG_EDGE.toString(), select.value);

    duplexSection.setSetting('duplexShortEdge', true);
    await eventToPromise('process-select-change', duplexSection);
    assertEquals(DuplexMode.SHORT_EDGE.toString(), select.value);
  });

  // Tests that checking the box or selecting a new option in the dropdown
  // updates the setting.
  test('select option', async () => {
    const checkbox = duplexSection.shadowRoot!.querySelector('cr-checkbox')!;
    const collapse = duplexSection.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(checkbox.checked);
    assertFalse(collapse.opened);
    assertFalse(duplexSection.getSettingValue('duplex') as boolean);
    assertFalse(duplexSection.getSettingValue('duplexShortEdge') as boolean);
    assertFalse(duplexSection.getSetting('duplex').setFromUi);
    assertFalse(duplexSection.getSetting('duplexShortEdge').setFromUi);

    checkbox.checked = true;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    assertTrue(collapse.opened);
    assertTrue(duplexSection.getSettingValue('duplex') as boolean);
    assertFalse(duplexSection.getSettingValue('duplexShortEdge') as boolean);
    assertTrue(duplexSection.getSetting('duplex').setFromUi);
    assertFalse(duplexSection.getSetting('duplexShortEdge').setFromUi);

    const select = duplexSection.shadowRoot!.querySelector('select')!;
    assertEquals(DuplexMode.LONG_EDGE.toString(), select.value);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(duplexSection, DuplexMode.SHORT_EDGE.toString());
    assertTrue(duplexSection.getSettingValue('duplex') as boolean);
    assertTrue(duplexSection.getSettingValue('duplexShortEdge') as boolean);
    assertTrue(duplexSection.getSetting('duplex').setFromUi);
    assertTrue(duplexSection.getSetting('duplexShortEdge').setFromUi);
  });

  // <if expr="is_chromeos">
  // Tests that if settings are enforced by enterprise policy the
  // appropriate UI is disabled.
  test('disabled by policy', function() {
    const checkbox = duplexSection.shadowRoot!.querySelector('cr-checkbox')!;
    assertFalse(checkbox.disabled);

    duplexSection.setSetting('duplex', true);
    const select = duplexSection.shadowRoot!.querySelector('select')!;
    assertFalse(select.disabled);

    model.set('settings.duplex.setByPolicy', true);
    assertTrue(checkbox.disabled);
    assertFalse(select.disabled);

    model.set('settings.duplexShortEdge.setByPolicy', true);
    assertTrue(checkbox.disabled);
    assertTrue(select.disabled);
  });
  // </if>
});
