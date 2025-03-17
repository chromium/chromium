// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewModelElement, PrintPreviewScalingSettingsElement} from 'chrome://print/print_preview.js';
import {ScalingType} from 'chrome://print/print_preview.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('ScalingSettingsInteractiveTest', function() {
  let scalingSection: PrintPreviewScalingSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
      'auto focus input', async () => {
        const scalingInput =
            scalingSection.shadowRoot!
                .querySelector('print-preview-number-settings-section')!.$
                .userValue.inputElement;
        const collapse =
            scalingSection.shadowRoot!.querySelector('cr-collapse')!;

        assertFalse(collapse.opened);
        assertEquals(
            ScalingType.DEFAULT, scalingSection.getSettingValue('scalingType'));

        // Select custom with the dropdown. This should autofocus the input.
        await Promise.all([
          selectOption(scalingSection, ScalingType.CUSTOM.toString()),
          eventToPromise('transitionend', collapse),
        ]);
        assertTrue(collapse.opened);
        assertEquals(scalingInput, getDeepActiveElement());

        // Blur and select default.
        scalingInput.blur();
        await Promise.all([
          selectOption(scalingSection, ScalingType.DEFAULT.toString()),
          eventToPromise('transitionend', collapse),
        ]);
        assertEquals(
            ScalingType.DEFAULT, scalingSection.getSettingValue('scalingType'));
        assertFalse(collapse.opened);

        // Set custom in JS, which happens when we set the sticky settings. This
        // should not autofocus the input.
        scalingSection.setSetting('scalingType', ScalingType.CUSTOM);
        await eventToPromise('transitionend', collapse);
        assertTrue(collapse.opened);
        assertNotEquals(scalingInput, getDeepActiveElement());
      });
});
