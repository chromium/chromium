// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {KeyInputState, MetaKey} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

function initInputKeyElement(): ShortcutInputKeyElement {
  const element = document.createElement('shortcut-input-key');
  document.body.appendChild(element);
  flush();
  return element;
}

// TODO(dpad, b/223455415): Migrate AllIconsHaveValidAriaLabelStringIds from
// shortcut customization before finishing the full migration.
suite('ShortcutInputKey', function() {
  let shortcutInputKeyElement: ShortcutInputKeyElement|null = null;

  setup(() => {
    // TODO(dpad, b/223455415): Provide a way to get the real loadTimeData in
    // ash_common browser tests.
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {
        'iconLabelOpenLauncher': 'launcher',
        'iconLabelOpenSearch': 'search',
        'iconLabelPrintScreen': 'take screenshot',
      };
    }
  });

  teardown(() => {
    if (shortcutInputKeyElement) {
      shortcutInputKeyElement.remove();
    }
    shortcutInputKeyElement = null;
  });

  test('BasicKeys', async () => {
    shortcutInputKeyElement = await initInputKeyElement();
    shortcutInputKeyElement.key = 'a';
    await flushTasks();

    const keyElement =
        shortcutInputKeyElement.shadowRoot!.querySelector('#key-text');
    assertTrue(!!keyElement);
    assertTrue(isVisible(keyElement));
    assertEquals('a', keyElement.textContent);

    const iconElement =
        shortcutInputKeyElement.shadowRoot!.querySelector('#key-icon');
    assertFalse(isVisible(iconElement));
  });

  test('IconKeys', async () => {
    shortcutInputKeyElement = initInputKeyElement();
    shortcutInputKeyElement.key = 'PrintScreen';
    await flushTasks();

    const iconElement = shortcutInputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    assertTrue(isVisible(iconElement));
    assertEquals('shortcut-input-keys:screenshot', iconElement.icon);

    const keyElement =
        shortcutInputKeyElement.shadowRoot!.querySelector('#key-text');
    assertFalse(isVisible(keyElement));
  });

  test('IconKeyHasAriaLabel', async () => {
    shortcutInputKeyElement = initInputKeyElement();
    shortcutInputKeyElement.key = 'PrintScreen';
    await flushTasks();

    const iconWrapperElement =
        shortcutInputKeyElement.shadowRoot!.querySelector('#key > div') as
        HTMLDivElement;
    assertTrue(isVisible(iconWrapperElement));

    assertEquals(
        'take screenshot',
        shortcutInputKeyElement.shadowRoot!.querySelector('#keyIcon')!
            .getAttribute('aria-label'));
  });

  test('MetaKeyShowLauncherIcon', async () => {
    shortcutInputKeyElement = initInputKeyElement();
    shortcutInputKeyElement.key = 'meta';
    shortcutInputKeyElement.metaKey = MetaKey.kLauncher;
    await flushTasks();

    // Should show launcher icon when metaKey is kLauncher.
    const iconElement = shortcutInputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    const iconWrapperElement =
        shortcutInputKeyElement.shadowRoot!.querySelector('#key > div') as
        HTMLDivElement;
    assertTrue(isVisible(iconElement));
    assertTrue(isVisible(iconWrapperElement));
    assertEquals('shortcut-input-keys:launcher', iconElement.icon);
    // Verify meta key's aria-label.
    assertEquals(
        'launcher',
        shortcutInputKeyElement.shadowRoot!.querySelector('#keyIcon')!
            .getAttribute('aria-label'));
  });

  test('MetaKeyShowSearchIcon', async () => {
    shortcutInputKeyElement = initInputKeyElement();
    shortcutInputKeyElement.key = 'meta';
    shortcutInputKeyElement.metaKey = MetaKey.kSearch;
    await flushTasks();

    // Should show search icon when metaKey is kSearch.
    const iconElement = shortcutInputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    const iconWrapperElement =
        shortcutInputKeyElement.shadowRoot!.querySelector('#key > div') as
        HTMLDivElement;
    assertTrue(isVisible(iconElement));
    assertTrue(isVisible(iconWrapperElement));
    assertEquals('shortcut-input-keys:search', iconElement.icon);
    // Verify meta key's aria-label.
    assertEquals(
        'search',
        shortcutInputKeyElement.shadowRoot!.querySelector('#keyIcon')!
            .getAttribute('aria-label'));
  });

  test('LwinKeyAsSearchModifier', async () => {
    shortcutInputKeyElement = initInputKeyElement();
    shortcutInputKeyElement.key = 'Meta';
    shortcutInputKeyElement.keyState = KeyInputState.ALPHANUMERIC_SELECTED;
    shortcutInputKeyElement.metaKey = MetaKey.kLauncher;
    await flushTasks();

    // Should show launcher icon when metaKey is kLauncher.
    const iconElement = shortcutInputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    assertEquals('shortcut-input-keys:launcher', iconElement.icon);
    // Lwin key should be treated as a search modifier key.
    assertEquals(
        KeyInputState.MODIFIER_SELECTED, shortcutInputKeyElement.keyState);
  });

  test('OtherKeyStateUnchanged', async () => {
    shortcutInputKeyElement = initInputKeyElement();
    shortcutInputKeyElement.key = 'a';
    shortcutInputKeyElement.keyState = KeyInputState.ALPHANUMERIC_SELECTED;
    shortcutInputKeyElement.metaKey = MetaKey.kLauncher;
    await flushTasks();

    // other keys should keep their original state.
    assertEquals(
        KeyInputState.ALPHANUMERIC_SELECTED, shortcutInputKeyElement.keyState);
  });
});
