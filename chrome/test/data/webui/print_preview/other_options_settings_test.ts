// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {CrCheckboxElement, PrintPreviewModelElement, PrintPreviewOtherOptionsSettingsElement, Settings} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OtherOptionsSettingsTest', function() {
  let otherOptionsSection: PrintPreviewOtherOptionsSettingsElement;

  let model: PrintPreviewModelElement;

  const keys: Array<keyof Settings> =
      ['headerFooter', 'cssBackground', 'rasterize', 'selectionOnly'];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.setSettingAvailableForTesting('headerFooter', true);
    model.setSetting('headerFooter', true, /*noSticky=*/ true);
    model.setSettingAvailableForTesting('cssBackground', true);
    model.setSetting('cssBackground', true, /*noSticky=*/ true);
    model.setSettingAvailableForTesting('selectionOnly', true);
    model.setSetting('selectionOnly', true, /*noSticky=*/ true);
    model.setSettingAvailableForTesting('rasterize', true);
    model.setSetting('rasterize', true, /*noSticky=*/ true);

    otherOptionsSection =
        document.createElement('print-preview-other-options-settings');
    otherOptionsSection.disabled = false;
    document.body.appendChild(otherOptionsSection);
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
  test('checkbox visibility', async function() {
    for (const setting of keys) {
      const checkbox =
          otherOptionsSection.shadowRoot.querySelector<CrCheckboxElement>(
              `#${setting}`)!;
      // Show, hide and reset.
      for (const value of [true, false, true]) {
        model.setSettingAvailableForTesting(setting, value);
        await microtasksFinished();
        // Element expected to be visible when available.
        assertEquals(!value, isSectionHidden(checkbox));
      }
    }
  });

  test('set with checkbox', async () => {
    async function testOptionCheckbox(settingName: keyof Settings):
        Promise<void> {
      const element =
          otherOptionsSection.shadowRoot.querySelector<CrCheckboxElement>(
              `#${settingName}`)!;
      const optionSetting = otherOptionsSection.getSetting(settingName);
      assertFalse(isSectionHidden(element));
      assertTrue(element.checked);
      assertTrue(optionSetting.value);
      assertFalse(optionSetting.setFromUi);
      element.click();

      const event =
          await eventToPromise('update-checkbox-setting', otherOptionsSection);
      assertEquals(element.id, event.detail);
      assertFalse(optionSetting.value);
      assertTrue(optionSetting.setFromUi);
    }

    await microtasksFinished();
    for (const setting of keys) {
      await testOptionCheckbox(setting);
    }
  });

  test('update from setting', async function() {
    for (const setting of keys) {
      const checkbox =
          otherOptionsSection.shadowRoot.querySelector<CrCheckboxElement>(
              `#${setting}`)!;
      // Set true and then false.
      for (const value of [true, false]) {
        otherOptionsSection.setSetting(setting, value);
        await microtasksFinished();
        // Element expected to be checked when setting is true.
        assertEquals(value, checkbox.checked);
      }
    }
  });

  // Tests that if settings are enforced by enterprise policy the checkbox
  // is disabled.
  test('header footer disabled by global policy', async function() {
    const checkbox =
        otherOptionsSection.shadowRoot.querySelector<CrCheckboxElement>(
            '#headerFooter')!;
    // Set true and then false.
    for (const value of [true, false]) {
      model.setSettingSetByGlobalPolicyForTesting('headerFooter', value);
      await microtasksFinished();
      // Element expected to be disabled when policy is set.
      assertEquals(value, checkbox.disabled);
    }
  });
});
