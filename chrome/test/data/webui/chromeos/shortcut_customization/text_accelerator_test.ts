// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/text_accelerator.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {KeyInputState} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString, stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {AcceleratorSource, LayoutStyle, TextAcceleratorPart, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';


function createTextAcceleratorPart(
    text: string, type: TextAcceleratorPartType): TextAcceleratorPart {
  return {text: stringToMojoString16(text), type};
}

suite('textAcceleratorTest', function() {
  let textAccelElement: TextAcceleratorElement|null = null;
  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    // Set up manager.
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
  });

  function getTextPartsContainer(): HTMLElement {
    return textAccelElement!.shadowRoot!.querySelector('.parts-container') as
        HTMLElement;
  }

  function getAllInputKeys(): NodeListOf<ShortcutInputKeyElement> {
    return getTextPartsContainer().querySelectorAll('shortcut-input-key');
  }

  function getAllPlainTextParts(): NodeListOf<HTMLSpanElement> {
    return getTextPartsContainer().querySelectorAll('span');
  }

  function getAllDelimiterParts(): NodeListOf<IronIconElement> {
    return getTextPartsContainer().querySelectorAll('#delimiter-icon');
  }

  function getLockIcon(): HTMLDivElement {
    return strictQuery(
        '.lock-icon-container', textAccelElement!.shadowRoot, HTMLDivElement);
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

  function initTextAcceleratorElement(
      parts: TextAcceleratorPart[] = [], source: AcceleratorSource,
      action: number, displayLockIcon: boolean): Promise<void> {
    textAccelElement = document.createElement('text-accelerator');
    textAccelElement.parts = parts;
    textAccelElement.source = source;
    textAccelElement.action = action;
    textAccelElement.displayLockIcon = displayLockIcon;
    document.body.appendChild(textAccelElement);
    return flushTasks();
  }


  test('TextAcceleratorPartsSingleModifier', async () => {
    const ctrlKey =
        createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
    await initTextAcceleratorElement(
        [ctrlKey], AcceleratorSource.kAmbient, 0, false);
    assertEquals(1, getTextPartsContainer().children.length);
    assertEquals(1, textAccelElement!.parts.length);
    const inputKey = getAllInputKeys()[0];
    assertEquals(inputKey!.key, mojoString16ToString(ctrlKey.text));
    assertEquals(inputKey!.keyState, KeyInputState.MODIFIER_SELECTED);
  });

  test('TextAcceleratorPartsSingleKey', async () => {
    const bKey = createTextAcceleratorPart('b', TextAcceleratorPartType.kKey);
    await initTextAcceleratorElement(
        [bKey], AcceleratorSource.kAmbient, 0, false);
    assertEquals(1, getTextPartsContainer().children.length);
    assertEquals(1, textAccelElement!.parts.length);
    const inputKey = getAllInputKeys()[0];
    assertEquals(inputKey!.key, mojoString16ToString(bKey.text));
    assertEquals(inputKey!.keyState, KeyInputState.ALPHANUMERIC_SELECTED);
  });

  test('TextAcceleratorPartsPlainText', async () => {
    const plainText = createTextAcceleratorPart(
        'Some text', TextAcceleratorPartType.kPlainText);
    await initTextAcceleratorElement(
        [plainText], AcceleratorSource.kAmbient, 0, false);
    assertEquals(1, getTextPartsContainer().children.length);
    const part = getAllPlainTextParts()[0];
    assertEquals(1, textAccelElement!.parts.length);
    assertEquals(part!.innerText, mojoString16ToString(plainText.text));
  });

  test('TextAcceleratorPartsDelimiter', async () => {
    const delimiter =
        createTextAcceleratorPart('+', TextAcceleratorPartType.kDelimiter);
    await initTextAcceleratorElement(
        [delimiter], AcceleratorSource.kAmbient, 0, false);
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
    await initTextAcceleratorElement(
        [ctrlKey, bKey, plainText, delimiter], AcceleratorSource.kAmbient, 0,
        false);
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

  test('LockIconVisibilityBasedOnProperties', async () => {
    const scenarios = [
      {customizationEnabled: true, displayLockIcon: true},
      {customizationEnabled: true, displayLockIcon: false},
      {customizationEnabled: false, displayLockIcon: false},
    ];
    // Prepare all test cases by looping the fakeLayoutInfo.
    const testCases = [];
    for (const layoutInfo of fakeLayoutInfo) {
      // If it's not text-accelerator, break the loop early.
      if (layoutInfo.style === LayoutStyle.kDefault) {
        continue;
      }
      for (const scenario of scenarios) {
        // replicate getSubcategory() logic.
        const subcategory = manager!.getAcceleratorSubcategory(
            layoutInfo.source, layoutInfo.action);
        const subcategoryIsUnlocked =
            !manager!.isSubcategoryLocked(subcategory);
        // replicate shouldShowLockIcon() logic.
        const expectLockIconVisible = scenario.customizationEnabled &&
            !scenario.displayLockIcon && subcategoryIsUnlocked;
        testCases.push({
          ...scenario,
          layoutInfo: layoutInfo,
          expectLockIconVisible: expectLockIconVisible,
        });
      }
    }
    // Verify lock icon show/hide based on properties.
    for (const testCase of testCases) {
      loadTimeData.overrideValues(
          {isCustomizationAllowed: testCase.customizationEnabled});
      const ctrlKey =
          createTextAcceleratorPart('ctrl', TextAcceleratorPartType.kModifier);
      await initTextAcceleratorElement(
          [ctrlKey], testCase.layoutInfo.source, testCase.layoutInfo.action,
          testCase.displayLockIcon);
      await flush();
      assertEquals(testCase.expectLockIconVisible, isVisible(getLockIcon()));
    }
  });
});
