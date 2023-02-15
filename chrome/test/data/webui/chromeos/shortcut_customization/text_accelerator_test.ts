// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/text_accelerator.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {mojoString16ToString, stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {TextAcceleratorPart, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';


function createTextAcceleratorPart(
    text: string, type: TextAcceleratorPartType): TextAcceleratorPart {
  return {text: stringToMojoString16(text), type};
}

suite('textAcceleratorTest', function() {
  let textAccelElement: TextAcceleratorElement|null = null;

  function getTextPartsContainer(): HTMLElement {
    return textAccelElement!.shadowRoot!.querySelector('.parts-container') as
        HTMLElement;
  }

  function getAllInputKeys(): NodeListOf<InputKeyElement> {
    return getTextPartsContainer().querySelectorAll('input-key');
  }

  function getAllPlainTextParts(): NodeListOf<HTMLSpanElement> {
    return getTextPartsContainer().querySelectorAll('span');
  }

  function getAllDelimiterParts(): NodeListOf<IronIconElement> {
    return getTextPartsContainer().querySelectorAll('#delimiter-icon');
  }

  function getLockIcon(): IronIconElement {
    return textAccelElement!.shadowRoot!.querySelector('#lock') as
        IronIconElement;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    if (textAccelElement) {
      textAccelElement.remove();
    }
    textAccelElement = null;
  });

  function initTextAcceleratorElement(parts: TextAcceleratorPart[] = []):
      Promise<void> {
    textAccelElement = document.createElement('text-accelerator');
    textAccelElement.parts = parts;
    document.body.appendChild(textAccelElement);
    return flushTasks();
  }


  test('TextAcceleratorPartsSingleModifier', async () => {
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    await initTextAcceleratorElement([ctrlKey]);
    assertEquals(1, getTextPartsContainer().children.length);
    assertEquals(1, textAccelElement!.parts.length);
    const inputKey = getAllInputKeys()[0];
    assertEquals(inputKey!.key, mojoString16ToString(ctrlKey.text));
    assertEquals(inputKey!.keyState, KeyInputState.MODIFIER_SELECTED);
  });

  test('TextAcceleratorPartsSingleKey', async () => {
    const bKey = createTextAcceleratorPart('b', TextAcceleratorPartType.kKey);
    await initTextAcceleratorElement([bKey]);
    assertEquals(1, getTextPartsContainer().children.length);
    assertEquals(1, textAccelElement!.parts.length);
    const inputKey = getAllInputKeys()[0];
    assertEquals(inputKey!.key, mojoString16ToString(bKey.text));
    assertEquals(inputKey!.keyState, KeyInputState.ALPHANUMERIC_SELECTED);
  });

  test('TextAcceleratorPartsPlainText', async () => {
    const plainText = createTextAcceleratorPart(
        'Some text', TextAcceleratorPartType.kPlainText);
    await initTextAcceleratorElement([plainText]);
    assertEquals(1, getTextPartsContainer().children.length);
    const part = getAllPlainTextParts()[0];
    assertEquals(1, textAccelElement!.parts.length);
    assertEquals(part!.innerText, mojoString16ToString(plainText.text));
  });

  test('TextAcceleratorPartsDelimiter', async () => {
    const delimiter =
        createTextAcceleratorPart('+', TextAcceleratorPartType.kDelimiter);
    await initTextAcceleratorElement([delimiter]);
    assertEquals(1, getTextPartsContainer().children.length);
    const delimiterPart = getAllDelimiterParts()[0];
    assertEquals(1, textAccelElement!.parts.length);
    assertEquals(delimiterPart!.icon, 'shortcut-customization-keys:plus');
  });

  test('TextAcceleratorPartsAll', async () => {
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    const bKey = createTextAcceleratorPart('b', TextAcceleratorPartType.kKey);
    const plainText = createTextAcceleratorPart(
        'Some text', TextAcceleratorPartType.kPlainText);
    const delimiter =
        createTextAcceleratorPart('+', TextAcceleratorPartType.kDelimiter);
    await initTextAcceleratorElement([ctrlKey, bKey, plainText, delimiter]);
    assertEquals(4, getTextPartsContainer().children.length);
    assertEquals(4, textAccelElement!.parts.length);

    const [ctrlInputKey, bInputKey] = getAllInputKeys();
    assertEquals(ctrlInputKey!.key, mojoString16ToString(ctrlKey.text));
    assertEquals(ctrlInputKey!.keyState, KeyInputState.MODIFIER_SELECTED);

    assertEquals(bInputKey!.key, mojoString16ToString(bKey.text));
    assertEquals(bInputKey!.keyState, KeyInputState.ALPHANUMERIC_SELECTED);
    const part = getAllPlainTextParts()[0];
    assertEquals(part!.innerText, mojoString16ToString(plainText.text));

    const delimiterPart = getAllDelimiterParts()[0];
    assertEquals(delimiterPart!.icon, 'shortcut-customization-keys:plus');
  });

  test('LockIconPresentWhenCustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    await initTextAcceleratorElement([ctrlKey]);
    assertTrue(isVisible(getLockIcon()));
  });

  test('LockIconHiddenWhenCustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    await initTextAcceleratorElement([ctrlKey]);
    assertFalse(isVisible(getLockIcon()));
  });
});
