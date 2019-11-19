// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScalingType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {selectOption} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

window.scaling_settings_interactive_test = {};
scaling_settings_interactive_test.suiteName = 'ScalingSettingsInteractiveTest';
/** @enum {string} */
scaling_settings_interactive_test.TestNames = {
  AutoFocusInput: 'auto focus input',
};

suite(scaling_settings_interactive_test.suiteName, function() {
  /** @type {?PrintPreviewScalingSettingsElement} */
  let scalingSection = null;

  /** @type {?PrintPreviewModelElement} */
  let model = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.scalingTypePdf.available', false);

    scalingSection = document.createElement('print-preview-scaling-settings');
    scalingSection.settings = model.settings;
    scalingSection.disabled = false;
    scalingSection.isPdf = false;
    fakeDataBind(model, scalingSection, 'settings');
    document.body.appendChild(scalingSection);
  });

  test(
      assert(scaling_settings_interactive_test.TestNames.AutoFocusInput),
      async () => {
        const scalingInput =
            scalingSection.$$('print-preview-number-settings-section')
                .$.userValue.inputElement;
        const scalingDropdown = scalingSection.$$('.md-select');
        const collapse = scalingSection.$$('iron-collapse');

        assertFalse(collapse.opened);
        assertEquals(
            ScalingType.DEFAULT, scalingSection.getSettingValue('scalingType'));

        // Select custom with the dropdown. This should autofocus the input.
        await Promise.all([
          selectOption(
              scalingSection, scalingSection.ScalingValue.CUSTOM.toString()),
          eventToPromise('transitionend', collapse),
        ]);
        assertTrue(collapse.opened);
        assertEquals(scalingInput, getDeepActiveElement());

        // Blur and select default.
        scalingInput.blur();
        await Promise.all([
          selectOption(
              scalingSection, scalingSection.ScalingValue.DEFAULT.toString()),
          eventToPromise('transitionend', collapse),
        ]);
        assertEquals(
            ScalingType.DEFAULT, scalingSection.getSettingValue('scalingType'));
        assertFalse(scalingSection.$$('iron-collapse').opened);

        // Set custom in JS, which happens when we set the sticky settings. This
        // should not autofocus the input.
        scalingSection.setSetting('scalingType', ScalingType.CUSTOM);
        await eventToPromise('transitionend', collapse);
        assertTrue(collapse.opened);
        assertNotEquals(scalingInput, getDeepActiveElement());
      });
});
