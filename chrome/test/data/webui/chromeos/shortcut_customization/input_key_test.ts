// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/strings.m.js';
import 'chrome://shortcut-customization/js/input_key.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {keyToIconNameMap} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

function initInputKeyElement(): InputKeyElement {
  const element = document.createElement('input-key');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('inputKeyTest', function() {
  let inputKeyElement: InputKeyElement|null = null;
  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();
  });

  teardown(() => {
    if (inputKeyElement) {
      inputKeyElement.remove();
    }
    if (manager) {
      manager.reset();
    }
    inputKeyElement = null;
  });

  test('BasicKeys', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'a';
    await flush();

    const keyElement = inputKeyElement.shadowRoot!.querySelector('#key-text');
    assertTrue(!!keyElement);
    assertTrue(isVisible(keyElement));
    assertEquals('a', keyElement.textContent);

    const iconElement = inputKeyElement.shadowRoot!.querySelector('#key-icon');
    assertFalse(isVisible(iconElement));
  });

  test('IconKeys', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'PrintScreen';
    await flush();

    const iconElement = inputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    assertTrue(isVisible(iconElement));
    assertEquals('shortcut-customization-keys:screenshot', iconElement.icon);

    const keyElement = inputKeyElement.shadowRoot!.querySelector('#key-text');
    assertFalse(isVisible(keyElement));
  });

  test('AllIconsHaveValidAriaLabelStringIds', async () => {
    inputKeyElement = initInputKeyElement();
    for (const keyCode of Object.keys(keyToIconNameMap)) {
      const ariaLabelStringId =
          InputKeyElement.getAriaLabelStringId(keyCode, true);
      assertTrue(
          inputKeyElement.i18nExists(ariaLabelStringId),
          `String ID ${ariaLabelStringId} should exist, but it doesn't.`);
    }
  });

  test('IconKeyHasAriaLabel', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'PrintScreen';
    await flush();

    const iconWrapperElement = inputKeyElement.shadowRoot!.querySelector(
                                   '#key > div') as HTMLDivElement;
    assertTrue(isVisible(iconWrapperElement));
    const iconDescriptionElement = inputKeyElement.shadowRoot!.querySelector(
                                       '#icon-description') as HTMLDivElement;
    assertEquals('take screenshot', iconDescriptionElement.textContent);
  });

  test('MetaKeyShowLauncherIcon', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'meta';

    manager!.setHasLauncherButton(true);
    await flush();

    // Should show launcher icon when hasLauncherButton is true.
    const iconElement = inputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    const iconWrapperElement = inputKeyElement.shadowRoot!.querySelector(
                                   '#key > div') as HTMLDivElement;
    assertTrue(isVisible(iconElement));
    assertTrue(isVisible(iconWrapperElement));
    assertEquals('shortcut-customization-keys:launcher', iconElement.icon);
    const iconDescriptionElement = inputKeyElement.shadowRoot!.querySelector(
                                       '#icon-description') as HTMLDivElement;
    assertEquals('launcher', iconDescriptionElement.textContent);
  });

  test('MetaKeyShowSearchIcon', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'meta';

    manager!.setHasLauncherButton(false);
    await flush();

    // Should show search icon when hasLauncherButton is false.
    const iconElement = inputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    const iconWrapperElement = inputKeyElement.shadowRoot!.querySelector(
                                   '#key > div') as HTMLDivElement;
    assertTrue(isVisible(iconElement));
    assertTrue(isVisible(iconWrapperElement));
    assertEquals('shortcut-customization-keys:search', iconElement.icon);

    const iconDescriptionElement = inputKeyElement.shadowRoot!.querySelector(
                                       '#icon-description') as HTMLDivElement;
    assertEquals('search', iconDescriptionElement.textContent);
  });

  test('LwinKeyAsSearchModifier', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'Meta';
    inputKeyElement.keyState = KeyInputState.ALPHANUMERIC_SELECTED;

    manager!.setHasLauncherButton(true);
    await flush();

    // Should show launcher icon when hasLauncherButton is true.
    const iconElement = inputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    assertEquals('shortcut-customization-keys:launcher', iconElement.icon);
    // Lwin key should be treated as a search modifier key.
    assertEquals(KeyInputState.MODIFIER_SELECTED, inputKeyElement.keyState);
  });

  test('OtherKeyStateUnchanged', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'a';
    inputKeyElement.keyState = KeyInputState.ALPHANUMERIC_SELECTED;

    manager!.setHasLauncherButton(true);
    await flush();

    // other keys should keep their original state.
    assertEquals(KeyInputState.ALPHANUMERIC_SELECTED, inputKeyElement.keyState);
  });

  test('AriaHiddenForSelectedKeys', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'a';
    inputKeyElement.keyState = KeyInputState.ALPHANUMERIC_SELECTED;

    const keyElement =
        strictQuery('#key-text', inputKeyElement.shadowRoot, HTMLSpanElement);
    assertTrue(!!keyElement);
    assertEquals('false', keyElement.ariaHidden);
  });

  test('AriaHiddenForUnselectedKeys', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'a';
    inputKeyElement.keyState = KeyInputState.NOT_SELECTED;

    const keyElement =
        strictQuery('#key-text', inputKeyElement.shadowRoot, HTMLSpanElement);
    assertTrue(!!keyElement);
    assertEquals('true', keyElement.ariaHidden);
  });
});