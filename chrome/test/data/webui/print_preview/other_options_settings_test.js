// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

suite('OtherOptionsSettingsTest', function() {
  /** @type {?PrintPreviewOtherOptionsSettingsElement} */
  let otherOptionsSection = null;

  /** @type {?PrintPreviewModelElement} */
  let model = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
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
   * @param {!CrCheckboxElement} checkbox The checkbox to check
   * @return {boolean} Whether the checkbox's parent section is hidden.
   */
  function isSectionHidden(checkbox) {
    return checkbox.parentNode.parentNode.hidden;
  }

  // Verifies that the correct checkboxes are hidden when different settings
  // are not available.
  test('checkbox visibility', function() {
    ['headerFooter', 'cssBackground', 'rasterize', 'selectionOnly'].forEach(
        setting => {
          const checkbox = otherOptionsSection.$$(`#${setting}`);
          // Show, hide and reset.
          [true, false, true].forEach(value => {
            model.set(`settings.${setting}.available`, value);
            // Element expected to be visible when available.
            assertEquals(!value, isSectionHidden(checkbox));
          });
        });
  });

  test('set with checkbox', async () => {
    const testOptionCheckbox = (settingName) => {
      const element = otherOptionsSection.$$('#' + settingName);
      const optionSetting = otherOptionsSection.getSetting(settingName);
      assertFalse(isSectionHidden(element));
      assertTrue(element.checked);
      assertTrue(optionSetting.value);
      assertFalse(optionSetting.setFromUi);
      element.checked = false;
      element.dispatchEvent(new CustomEvent('change'));
      return eventToPromise('update-checkbox-setting', otherOptionsSection)
          .then(function(event) {
            assertEquals(element.id, event.detail);
            assertFalse(optionSetting.value);
            assertTrue(optionSetting.setFromUi);
          });
    };

    await testOptionCheckbox('headerFooter');
    await testOptionCheckbox('cssBackground');
    await testOptionCheckbox('rasterize');
    await testOptionCheckbox('selectionOnly');
  });

  test('update from setting', function() {
    ['headerFooter', 'cssBackground', 'rasterize', 'selectionOnly'].forEach(
        setting => {
          const checkbox = otherOptionsSection.$$(`#${setting}`);
          // Set true and then false.
          [true, false].forEach(value => {
            otherOptionsSection.setSetting(setting, value);
            // Element expected to be checked when setting is true.
            assertEquals(value, checkbox.checked);
          });
        });
  });

  // Tests that if settings are enforced by enterprise policy the checkbox
  // is disabled.
  test('header footer disabled by policy', function() {
    const checkbox = otherOptionsSection.$$('#headerFooter');
    // Set true and then false.
    [true, false].forEach(value => {
      model.set('settings.headerFooter.setByPolicy', value);
      // Element expected to be disabled when policy is set.
      assertEquals(value, checkbox.disabled);
    });
  });
});
