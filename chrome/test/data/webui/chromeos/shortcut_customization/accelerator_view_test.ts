// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_view.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {setShortcutProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorResultData} from 'chrome://shortcut-customization/mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createStandardAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

export function initAcceleratorViewElement(): AcceleratorViewElement {
  const element = document.createElement('accelerator-view');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('acceleratorViewTest', function() {
  let viewElement: AcceleratorViewElement|null = null;

  let manager: AcceleratorLookupManager|null = null;
  let provider: FakeShortcutProvider;

  setup(() => {
    provider = new FakeShortcutProvider();
    setShortcutProviderForTesting(provider);

    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }

    if (viewElement) {
      viewElement.remove();
    }
    viewElement = null;
  });

  function getInputKey(selector: string): InputKeyElement {
    const element = viewElement!.shadowRoot!.querySelector(selector);
    assertTrue(!!element);
    return element as InputKeyElement;
  }

  test('LoadsBasicAccelerator', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    viewElement.acceleratorInfo = acceleratorInfo;
    await flush();
    const keys = viewElement.shadowRoot!.querySelectorAll('input-key');
    // Three keys: shift, control, g
    assertEquals(3, keys.length);

    assertEquals(
        'ctrl',
        keys[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'shift',
        keys[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
  });

  test('EditableAccelerator', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flush();

    const ctrlKey = getInputKey('#ctrlKey');
    const altKey = getInputKey('#altKey');
    const shiftKey = getInputKey('#shiftKey');
    const metaKey = getInputKey('#searchKey');
    const pendingKey = getInputKey('#pendingKey');

    // By default, no keys should be registered.
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Simulate Ctrl + Alt + e.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'e',
      keyCode: 69,
      code: 'KeyE',
      ctrlKey: true,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();

    assertEquals('modifier-selected', ctrlKey.keyState);
    assertEquals('modifier-selected', altKey.keyState);
    assertEquals('not-selected', shiftKey.keyState);
    assertEquals('not-selected', metaKey.keyState);
    assertEquals('alpha-numeric-selected', pendingKey.keyState);
    assertEquals('e', pendingKey.key);
  });

  test('LockIconVisibleWhenCustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    viewElement = initAcceleratorViewElement();
    await flushTasks();
    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    viewElement.acceleratorInfo = acceleratorInfo;

    // This set() call is necessary to notify the element that a sub-property
    // has been updated.
    viewElement.set('acceleratorInfo.locked', false);
    viewElement.sourceIsLocked = false;
    await flushTasks();
    let lockItemContainer = viewElement.shadowRoot!.querySelector(
                                '#lockIconContainer') as HTMLDivElement;
    // If customization is enabled, the acceleratorInfo is not locked, and the
    // sourceIsLocked property is false, we expect the lock icon to be hidden.
    assertTrue(lockItemContainer.hidden);

    viewElement.set('acceleratorInfo.locked', true);
    viewElement.sourceIsLocked = false;
    await flushTasks();
    lockItemContainer = viewElement.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;
    // If customization is enabled, the acceleratorInfo is locked, and the
    // sourceIsLocked property is false, we expect the lock icon to be visible.
    assertFalse(lockItemContainer.hidden);

    viewElement.set('acceleratorInfo.locked', false);
    viewElement.sourceIsLocked = true;
    await flushTasks();
    lockItemContainer = viewElement.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;
    // If customization is enabled, the acceleratorInfo is not locked, and the
    // sourceIsLocked property is true, we expect the lock icon to be visible.
    assertFalse(lockItemContainer.hidden);

    viewElement.set('acceleratorInfo.locked', true);
    viewElement.sourceIsLocked = true;
    await flushTasks();
    lockItemContainer = viewElement.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;
    // If customization is enabled, the acceleratorInfo is locked, and the
    // sourceIsLocked property is true, we expect the lock icon to be visible.
    assertFalse(lockItemContainer.hidden);
  });

  test('LockIconHiddenWhenCustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    viewElement = initAcceleratorViewElement();
    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    viewElement.acceleratorInfo = acceleratorInfo;

    viewElement.set('acceleratorInfo.locked', false);
    viewElement.sourceIsLocked = false;
    await flush();

    let lockItemContainer = viewElement.shadowRoot!.querySelector(
                                '#lockIconContainer') as HTMLDivElement;

    // If customization is disabled, the lock icon should always be hidden,
    // regardless of the acceleratorInfo.locked or sourceIsLocked properties.
    assertTrue(lockItemContainer.hidden);

    viewElement.set('acceleratorInfo.locked', true);
    viewElement.sourceIsLocked = false;
    await flush();
    lockItemContainer = viewElement.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;

    // If customization is disabled, the lock icon should always be hidden,
    // regardless of the acceleratorInfo.locked or sourceIsLocked properties.
    assertTrue(lockItemContainer.hidden);

    viewElement.set('acceleratorInfo.locked', false);
    viewElement.sourceIsLocked = true;
    await flush();
    lockItemContainer = viewElement.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;

    // If customization is disabled, the lock icon should always be hidden,
    // regardless of the acceleratorInfo.locked or sourceIsLocked properties.
    assertTrue(lockItemContainer.hidden);

    viewElement.set('acceleratorInfo.locked', true);
    viewElement.sourceIsLocked = true;
    await flush();
    lockItemContainer = viewElement.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;

    // If customization is disabled, the lock icon should always be hidden,
    // regardless of the acceleratorInfo.locked or sourceIsLocked properties.
    assertTrue(lockItemContainer.hidden);
  });

  test('ElementFocusableWhenCustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    viewElement.acceleratorInfo = acceleratorInfo;

    await flushTasks();
    const containerElement =
        viewElement.shadowRoot!.querySelector('#container') as HTMLDivElement;
    assertEquals(0, containerElement.tabIndex);
  });

  test('ElementNotFocusableWhenCustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    viewElement.acceleratorInfo = acceleratorInfo;

    await flushTasks();
    const containerElement =
        viewElement.shadowRoot!.querySelector('#container') as HTMLDivElement;
    assertEquals(-1, containerElement.tabIndex);
  });
});