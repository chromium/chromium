// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {CrCheckboxElement, PrintPreviewModelElement, PrintPreviewOtherOptionsSettingsElement, Settings} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('OtherOptionsSettingsTest', function() {
  let otherOptionsSection: PrintPreviewOtherOptionsSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.headerFooter.available', true);
    model.set('settings.headerFooter.value', true);
    model.set('settings.cssBackground.available', true);
    model.set('settings.cssBackground.value', true);
    model.set('settings.selectionOnly.available', true);
    model.set('settings.selectionOnly.value', true);
    model.set('settings.rasterize.available', true);
    model.set('settings.rasterize.value', true);

    otherOptionsSection =
        document.createElement('print-preview-other-options-settings');
    otherOptionsSection.settings = model.settings;
    otherOptionsSection.disabled = false;
    fakeDataBind(model, otherOptionsSection, 'settings');
    document.body.appendChild(otherOptionsSection);
    flush();
  });

  /**
   * @param checkbox The checkbox to check
   * @return Whether the checkbox's parent section is hidden.
   */
  function isSectionHidden(checkbox: CrCheckboxElement): boolean {
    return (checkbox.parentNode!.parentNode! as HTMLElement).hidden;
  }

  // Verifies that the correct checkboxes are hidden when different settings
  // are not available.
  test('checkbox visibility', function() {
    ['headerFooter', 'cssBackground', 'rasterize', 'selectionOnly'].forEach(
        setting => {
          const checkbox =
              otherOptionsSection.shadowRoot!.querySelector<CrCheckboxElement>(
                  `#${setting}`)!;
          // Show, hide and reset.
          [true, false, true].forEach(value => {
            model.set(`settings.${setting}.available`, value);
            // Element expected to be visible when available.
            assertEquals(!value, isSectionHidden(checkbox));
          });
        });
  });

  test('set with checkbox', async () => {
    function testOptionCheckbox(settingName: keyof Settings): Promise<void> {
      const element =
          otherOptionsSection.shadowRoot!.querySelector<CrCheckboxElement>(
              `#${settingName}`)!;
      const optionSetting = otherOptionsSection.getSetting(settingName);
      assertFalse(isSectionHidden(element));
      assertTrue(element.checked);
      assertTrue(optionSetting.value);
      assertFalse(optionSetting.setFromUi);
      element.checked = false;
      element.dispatchEvent(
          new CustomEvent('change', {bubbles: true, composed: true}));
      return eventToPromise('update-checkbox-setting', otherOptionsSection)
          .then(function(event: CustomEvent<string>) {
            assertEquals(element.id, event.detail);
            assertFalse(optionSetting.value);
            assertTrue(optionSetting.setFromUi);
          });
    }

    await testOptionCheckbox('headerFooter');
    await testOptionCheckbox('cssBackground');
    await testOptionCheckbox('rasterize');
    await testOptionCheckbox('selectionOnly');
  });

  test('update from setting', function() {
    const keys: Array<keyof Settings> =
        ['headerFooter', 'cssBackground', 'rasterize', 'selectionOnly'];
    keys.forEach(setting => {
      const checkbox =
          otherOptionsSection.shadowRoot!.querySelector<CrCheckboxElement>(
              `#${setting}`)!;
      // Set true and then false.
      [true, false].forEach((value: boolean) => {
        otherOptionsSection.setSetting(setting, value);
        // Element expected to be checked when setting is true.
        assertEquals(value, checkbox.checked);
      });
    });
  });

  // Tests that if settings are enforced by enterprise policy the checkbox
  // is disabled.
  test('header footer disabled by policy', function() {
    const checkbox =
        otherOptionsSection.shadowRoot!.querySelector<CrCheckboxElement>(
            '#headerFooter')!;
    // Set true and then false.
    [true, false].forEach((value: boolean) => {
      model.set('settings.headerFooter.setByPolicy', value);
      // Element expected to be disabled when policy is set.
      assertEquals(value, checkbox.disabled);
    });
  });
});
