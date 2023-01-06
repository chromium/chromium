// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/text_accelerator.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {mojoString16ToString, stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {TextAcceleratorPart, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

function createTextAcceleratorPart(
    text: string, type: TextAcceleratorPartType): TextAcceleratorPart {
  return {text: stringToMojoString16(text), type};
}

export function initTextAcceleratorElement(parts: TextAcceleratorPart[] = []):
    TextAcceleratorElement {
  const element: TextAcceleratorElement =
      document.createElement('text-accelerator');
  element.parts = parts;
  document.body.appendChild(element);
  flush();
  return element;
}

suite('textAcceleratorTest', function() {
  let textAccelElement: TextAcceleratorElement|null = null;

  function getTextWrapperEl(): HTMLElement {
    return textAccelElement!.shadowRoot!.querySelector('#text-wrapper') as
        HTMLElement;
  }

  function getAllInputKeys(): NodeListOf<InputKeyElement> {
    return getTextWrapperEl().querySelectorAll('input-key');
  }

  teardown(() => {
    if (textAccelElement) {
      textAccelElement.remove();
    }
    textAccelElement = null;
  });

  test('TextAcceleratorPartsSingleModifier', () => {
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    textAccelElement = initTextAcceleratorElement([ctrlKey]);
    assertEquals(1, textAccelElement.parts.length);
    const inputKey = getAllInputKeys()[0];
    assertEquals(inputKey!.key, mojoString16ToString(ctrlKey.text));
    assertEquals(inputKey!.keyState, KeyInputState.MODIFIER_SELECTED);
  });

  test('TextAcceleratorPartsSingleKey', () => {
    const bKey = createTextAcceleratorPart('b', TextAcceleratorPartType.kKey);
    textAccelElement = initTextAcceleratorElement([bKey]);
    assertEquals(1, textAccelElement.parts.length);
    const inputKey = getAllInputKeys()[0];
    assertEquals(inputKey!.key, mojoString16ToString(bKey.text));
    assertEquals(inputKey!.keyState, KeyInputState.ALPHANUMERIC_SELECTED);
  });

  test('TextAcceleratorPartsPlainText', () => {
    const plainText = createTextAcceleratorPart(
        'Some text', TextAcceleratorPartType.kPlainText);
    textAccelElement = initTextAcceleratorElement([plainText]);
    assertEquals(1, textAccelElement.parts.length);
    assertEquals(
        getTextWrapperEl().innerText, mojoString16ToString(plainText.text));
  });

  test('TextAcceleratorPartsAll', () => {
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    const bKey = createTextAcceleratorPart('b', TextAcceleratorPartType.kKey);
    const plainText = createTextAcceleratorPart(
        'Some text', TextAcceleratorPartType.kPlainText);
    textAccelElement = initTextAcceleratorElement([ctrlKey, bKey, plainText]);
    assertEquals(3, textAccelElement.parts.length);

    const [ctrlInputKey, bInputKey] = getAllInputKeys();
    assertEquals(ctrlInputKey!.key, mojoString16ToString(ctrlKey.text));
    assertEquals(ctrlInputKey!.keyState, KeyInputState.MODIFIER_SELECTED);

    assertEquals(bInputKey!.key, mojoString16ToString(bKey.text));
    assertEquals(bInputKey!.keyState, KeyInputState.ALPHANUMERIC_SELECTED);
    assertTrue(getTextWrapperEl().innerText.includes(
        mojoString16ToString(plainText.text)));
  });
});
