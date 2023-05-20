// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_view.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {setShortcutProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, LayoutStyle, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {isCategoryLocked} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {AcceleratorResultData} from 'chrome://shortcut-customization/mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

  function getLockIcon(): HTMLDivElement {
    return strictQuery(
        '.lock-icon-container', viewElement!.shadowRoot, HTMLDivElement);
  }

  function getEditIcon(): HTMLDivElement {
    return strictQuery(
        '#editIconContainer', viewElement!.shadowRoot, HTMLDivElement);
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

  test('EditWithFunctionKeyAsOnlyKey', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flushTasks();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flushTasks();

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

    // Simulate F3.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'F3',
      keyCode: 114,
      code: 'F3',
      ctrlKey: false,
      altKey: false,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.ALPHANUMERIC_SELECTED, pendingKey.keyState);
    assertEquals('f3', pendingKey.key);
  });

  test('LockIconVisibilityBasedOnProperties', async () => {
    const scenarios = [
      {customizationEnabled: true, locked: true, sourceIsLocked: true},
      {customizationEnabled: true, locked: true, sourceIsLocked: false},
      {customizationEnabled: true, locked: false, sourceIsLocked: true},
      {customizationEnabled: true, locked: false, sourceIsLocked: false},
      {customizationEnabled: false, locked: true, sourceIsLocked: true},
      {customizationEnabled: false, locked: true, sourceIsLocked: false},
      {customizationEnabled: false, locked: false, sourceIsLocked: true},
      {customizationEnabled: false, locked: false, sourceIsLocked: false},
    ];

    // Prepare all test cases by looping the fakeLayoutInfo.
    const testCases = [];
    for (const layoutInfo of fakeLayoutInfo) {
      // If it's text accelerator, break the loop early.
      if (layoutInfo.style !== LayoutStyle.kDefault) {
        continue;
      }
      for (const scenario of scenarios) {
        // replicate getCategory() logic.
        const category = manager!.getAcceleratorCategory(
            layoutInfo.source, layoutInfo.action);
        // replicate shouldShowLockIcon() logic.
        const expectLockIconVisible = scenario.customizationEnabled &&
            !isCategoryLocked(category) &&
            (scenario.locked || scenario.sourceIsLocked);
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
          {isCustomizationEnabled: testCase.customizationEnabled});
      viewElement = initAcceleratorViewElement();
      viewElement.source = testCase.layoutInfo.source;
      viewElement.action = testCase.layoutInfo.action;
      const acceleratorInfo = createStandardAcceleratorInfo(
          Modifier.CONTROL | Modifier.SHIFT,
          /*key=*/ 71,
          /*keyDisplay=*/ 'g');
      viewElement.acceleratorInfo = acceleratorInfo;
      viewElement.set('acceleratorInfo.locked', testCase.locked);
      viewElement.sourceIsLocked = testCase.sourceIsLocked;

      await flush();
      assertEquals(testCase.expectLockIconVisible, isVisible(getLockIcon()));
    }
  });

  test('EditIconVisibilityBasedOnProperties', async () => {
    // Mainly test on customizationEnabled and accelerator is not locked.
    const scenarios = [
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: false,
      },
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: true,
      },
      {
        customizationEnabled: true,
        locked: true,
        sourceIsLocked: false,
        isAcceleratorRow: false,
      },
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: true,
        isAcceleratorRow: true,
      },
      {
        customizationEnabled: false,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: false,
      },
    ];

    // Prepare all test cases by looping the fakeLayoutInfo.
    const testCases = [];
    for (const layoutInfo of fakeLayoutInfo) {
      // If it's text accelerator, break the loop early.
      if (layoutInfo.style !== LayoutStyle.kDefault) {
        continue;
      }
      for (const scenario of scenarios) {
        // replicate getCategory() logic.
        const category = manager!.getAcceleratorCategory(
            layoutInfo.source, layoutInfo.action);
        // replicate shouldShowLockIcon() logic.
        const expectEditIconVisible = scenario.customizationEnabled &&
            scenario.isAcceleratorRow && !isCategoryLocked(category) &&
            !scenario.locked && !scenario.sourceIsLocked;
        testCases.push({
          ...scenario,
          layoutInfo: layoutInfo,
          expectEditIconVisible: expectEditIconVisible,
        });
      }
    }
    for (const testCase of testCases) {
      loadTimeData.overrideValues(
          {isCustomizationEnabled: testCase.customizationEnabled});
      viewElement = initAcceleratorViewElement();
      viewElement.source = testCase.layoutInfo.source;
      viewElement.action = testCase.layoutInfo.action;
      viewElement.showEditIcon = testCase.isAcceleratorRow;
      const acceleratorInfo = createStandardAcceleratorInfo(
          Modifier.CONTROL | Modifier.SHIFT,
          /*key=*/ 71,
          /*keyDisplay=*/ 'g');
      viewElement.acceleratorInfo = acceleratorInfo;
      viewElement.set('acceleratorInfo.locked', testCase.locked);
      viewElement.sourceIsLocked = testCase.sourceIsLocked;

      await flush();
      assertEquals(
          testCase.expectEditIconVisible,
          !getEditIcon().hasAttribute('hidden'));
    }
  });
});
