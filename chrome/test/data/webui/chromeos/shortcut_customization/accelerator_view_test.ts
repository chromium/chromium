// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_view.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeShortcutInputProvider} from 'chrome://resources/ash/common/shortcut_input_ui/fake_shortcut_input_provider.js';
import {KeyEvent} from 'chrome://resources/ash/common/shortcut_input_ui/input_device_settings.mojom-webui.js';
import {ShortcutInputElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import {ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {KeyInputState, Modifier as ModifierEnum} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeAcceleratorConfig, fakeDefaultAccelerators, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {setShortcutProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {setShortcutInputProviderForTesting} from 'chrome://shortcut-customization/js/shortcut_input_mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, LayoutStyle, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorResultData} from 'chrome://shortcut-customization/mojom-webui/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createStandardAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

export function initAcceleratorViewElement(): AcceleratorViewElement {
  const element = document.createElement('accelerator-view');
  // Set default acceleratorInfo and viewState
  element.acceleratorInfo = createUserAcceleratorInfo(
      Modifier.CONTROL | Modifier.SHIFT,
      /*key=*/ 71,
      /*keyDisplay=*/ 'g');
  element.viewState = ViewState.VIEW;
  document.body.appendChild(element);
  flush();
  return element;
}

suite('acceleratorViewTest', function() {
  let viewElement: AcceleratorViewElement|null = null;

  let manager: AcceleratorLookupManager|null = null;
  let provider: FakeShortcutProvider;
  const shortcutInputProvider: FakeShortcutInputProvider =
      new FakeShortcutInputProvider();

  setup(() => {
    provider = new FakeShortcutProvider();
    provider.setFakeGetDefaultAcceleratorsForId(fakeDefaultAccelerators);
    setShortcutProviderForTesting(provider);
    setShortcutInputProviderForTesting(shortcutInputProvider);

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

  function getPendingKeyElement(shortcutInputElement: ShortcutInputElement):
      ShortcutInputKeyElement {
    return strictQuery(
        '#pendingKey', shortcutInputElement!.shadowRoot,
        ShortcutInputKeyElement);
  }

  function getCtrlElement(shortcutInputElement: ShortcutInputElement):
      ShortcutInputKeyElement {
    return strictQuery(
        '#ctrlKey', shortcutInputElement!.shadowRoot, ShortcutInputKeyElement);
  }

  function getShiftElement(shortcutInputElement: ShortcutInputElement):
      ShortcutInputKeyElement {
    return strictQuery(
        '#shiftKey', shortcutInputElement!.shadowRoot, ShortcutInputKeyElement);
  }

  function getAltElement(shortcutInputElement: ShortcutInputElement):
      ShortcutInputKeyElement {
    return strictQuery(
        '#altKey', shortcutInputElement!.shadowRoot, ShortcutInputKeyElement);
  }

  function getSearchElement(shortcutInputElement: ShortcutInputElement):
      ShortcutInputKeyElement {
    return strictQuery(
        '#searchKey', shortcutInputElement!.shadowRoot,
        ShortcutInputKeyElement);
  }

  function getLockIcon(): HTMLDivElement {
    return strictQuery(
        '.lock-icon-container', viewElement!.shadowRoot, HTMLDivElement);
  }

  function getEditIcon(): HTMLDivElement {
    return strictQuery(
        '.edit-icon-container', viewElement!.shadowRoot, HTMLDivElement);
  }

  test('LoadsBasicAccelerator', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const keys = viewElement.shadowRoot!.querySelectorAll('shortcut-input-key');
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

    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flush();

    const shortcutInput = strictQuery(
        'shortcut-input', viewElement!.shadowRoot, ShortcutInputElement);

    let ctrlKey = getCtrlElement(shortcutInput);
    let altKey = getAltElement(shortcutInput);
    let metaKey = getSearchElement(shortcutInput);
    let shiftKey = getShiftElement(shortcutInput);
    let pendingKey = getPendingKeyElement(shortcutInput);

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

    // Simulate Ctrl.
    const keyEvent: KeyEvent = {
      vkey: VKey.kControl,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.CONTROL,
      keyDisplay: 'ctrl',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flush();

    assertEquals(KeyInputState.MODIFIER_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    // Release Ctrl, expect it to not be selected.
    const keyEventReleased: KeyEvent = {
      vkey: VKey.kControl,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'ctrl',
    };
    shortcutInputProvider.sendKeyReleaseEvent(
        keyEventReleased, keyEventReleased);

    await flush();

    ctrlKey = getCtrlElement(shortcutInput);
    altKey = getAltElement(shortcutInput);
    metaKey = getSearchElement(shortcutInput);
    shiftKey = getShiftElement(shortcutInput);
    pendingKey = getPendingKeyElement(shortcutInput);

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    const keyEvent2: KeyEvent = {
      vkey: VKey.kKeyE,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'e',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent2, keyEvent2);

    await flushTasks();
    pendingKey = getPendingKeyElement(shortcutInput);

    assertEquals(KeyInputState.ALPHANUMERIC_SELECTED, pendingKey.keyState);
    assertEquals('e', pendingKey.key);

    // Release `e`, expect it to not be selected.
    const keyEvent2Released: KeyEvent = {
      vkey: VKey.kKeyE,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: '',
    };
    shortcutInputProvider.sendKeyReleaseEvent(
        keyEvent2Released, keyEvent2Released);
    await flushTasks();

    ctrlKey = getCtrlElement(shortcutInput);
    altKey = getAltElement(shortcutInput);
    metaKey = getSearchElement(shortcutInput);
    shiftKey = getShiftElement(shortcutInput);
    pendingKey = getPendingKeyElement(shortcutInput);

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals('key', pendingKey.key);
  });

  test('EditWithFunctionKeyAsOnlyKey', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flushTasks();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flushTasks();

    const shortcutInput = strictQuery(
        'shortcut-input', viewElement!.shadowRoot, ShortcutInputElement);

    const ctrlKey = getCtrlElement(shortcutInput);
    const altKey = getAltElement(shortcutInput);
    const metaKey = getSearchElement(shortcutInput);
    const shiftKey = getShiftElement(shortcutInput);
    const pendingKey = getPendingKeyElement(shortcutInput);

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
    const keyEvent: KeyEvent = {
      vkey: VKey.kF3,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'f3',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flush();

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.ALPHANUMERIC_SELECTED, pendingKey.keyState);
    assertEquals('f3', pendingKey.key);
  });

  test('LockIconVisibilityBasedOnProperties', async () => {
    viewElement = initAcceleratorViewElement();
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
        const subcategory = manager!.getAcceleratorSubcategory(
            layoutInfo.source, layoutInfo.action);
        const subcategoryIsLocked = manager!.isSubcategoryLocked(subcategory);
        // replicate shouldShowLockIcon() logic.
        const expectLockIconVisible = scenario.customizationEnabled &&
            !subcategoryIsLocked &&
            (scenario.locked || scenario.sourceIsLocked);
        testCases.push({
          ...scenario,
          layoutInfo: layoutInfo,
          subcategoryIsLocked: subcategoryIsLocked,
          expectLockIconVisible: expectLockIconVisible,
        });
      }
    }
    // Verify lock icon show/hide based on properties.
    for (const testCase of testCases) {
      loadTimeData.overrideValues(
          {isCustomizationAllowed: testCase.customizationEnabled});
      viewElement.source = testCase.layoutInfo.source;
      viewElement.action = testCase.layoutInfo.action;
      viewElement.subcategoryIsLocked = testCase.subcategoryIsLocked;
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
    viewElement = initAcceleratorViewElement();
    // Mainly test on customizationEnabled and accelerator is not locked.
    const scenarios = [
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: false,
        isFirstAccelerator: true,
      },
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: true,
        isFirstAccelerator: true,
      },
      {
        customizationEnabled: true,
        locked: true,
        sourceIsLocked: false,
        isAcceleratorRow: false,
        isFirstAccelerator: true,
      },
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: true,
        isAcceleratorRow: true,
        isFirstAccelerator: false,
      },
      {
        customizationEnabled: false,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: false,
        isFirstAccelerator: true,
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
        // replicate getSubcategory() logic.
        const subcategory = manager!.getAcceleratorSubcategory(
            layoutInfo.source, layoutInfo.action);
        const subcategoryIsLocked = manager!.isSubcategoryLocked(subcategory);
        // replicate shouldShowLockIcon() logic.
        const expectEditIconVisible = scenario.customizationEnabled &&
            scenario.isAcceleratorRow && !subcategoryIsLocked &&
            !scenario.locked && !scenario.sourceIsLocked &&
            scenario.isFirstAccelerator;
        testCases.push({
          ...scenario,
          layoutInfo: layoutInfo,
          subcategoryIsLocked: subcategoryIsLocked,
          expectEditIconVisible: expectEditIconVisible,
        });
      }
    }
    for (const testCase of testCases) {
      loadTimeData.overrideValues(
          {isCustomizationAllowed: testCase.customizationEnabled});
      viewElement.source = testCase.layoutInfo.source;
      viewElement.action = testCase.layoutInfo.action;
      viewElement.subcategoryIsLocked = testCase.subcategoryIsLocked;
      viewElement.showEditIcon = testCase.isAcceleratorRow;
      viewElement.isFirstAccelerator = testCase.isFirstAccelerator;
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

  test('KeyDisplayAndIconDuringEdit', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();

    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;
    await flush();

    const shortcutInput = strictQuery(
        'shortcut-input', viewElement!.shadowRoot, ShortcutInputElement);
    const pendingKey = getPendingKeyElement(shortcutInput);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };
    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Simulate SHIFT + SPACE, expect the key display to be 'space'.
    const keyEvent: KeyEvent = {
      vkey: VKey.kSpace,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.SHIFT,
      keyDisplay: 'space',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flush();
    assertEquals('space', pendingKey.key);

    // Simulate SHIFT + OVERVIEW, expect the key display to be
    // 'LaunchApplication1' and the icon to be 'overview'.
    const keyEvent2: KeyEvent = {
      vkey: VKey.kF4,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.SHIFT,
      keyDisplay: 'LaunchApplication1',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent2, keyEvent2);

    await flush();

    assertEquals('LaunchApplication1', pendingKey.key);
    const keyIconElement =
        pendingKey.shadowRoot!.querySelector('#key-icon') as IronIconElement;
    assertEquals('shortcut-input-keys:overview', keyIconElement.icon);

    // Simulate SHIFT + BRIGHTNESS_UP, expect the key display to be
    // 'BrightnessUp' and the icon to be 'display-brightness-up'.
    const keyEvent3: KeyEvent = {
      vkey: VKey.kBrightnessUp,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.SHIFT,
      keyDisplay: 'BrightnessUp',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent3, keyEvent3);

    await flush();

    assertEquals('BrightnessUp', pendingKey.key);
    const keyIconElement2 =
        pendingKey.shadowRoot!.querySelector('#key-icon') as IronIconElement;
    assertEquals(
        'shortcut-input-keys:display-brightness-up', keyIconElement2.icon);

    // Simulate SHIFT + MUTE_MICROPHONE.
    const keyEvent4: KeyEvent = {
      vkey: VKey.kMicrophoneMuteToggle,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.SHIFT,
      keyDisplay: 'MicrophoneMuteToggle',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent4, keyEvent4);

    await flush();

    assertEquals('MicrophoneMuteToggle', pendingKey.key);
    const keyIconElement3 =
        pendingKey.shadowRoot!.querySelector('#key-icon') as IronIconElement;
    assertEquals('shortcut-input-keys:microphone-mute', keyIconElement3.icon);

    // Simulate CONTROL + BACKQUOTE.
    const keyEvent5: KeyEvent = {
      vkey: VKey.kOem3,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.SHIFT,
      keyDisplay: '`',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent5, keyEvent5);
    await flush();

    assertEquals('`', pendingKey.key);
  });

  test('GetAriaLabels', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.SHIFT | Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ 's');
    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    viewElement.viewState = ViewState.VIEW;
    await flush();

    let viewContainer =
        strictQuery('#container', viewElement.shadowRoot, HTMLDivElement);
    assertEquals('alt shift s', viewContainer.ariaLabel);

    // Aria label is empty during editing process.
    viewElement.viewState = ViewState.EDIT;
    await flush();

    viewContainer =
        strictQuery('#container', viewElement.shadowRoot, HTMLDivElement);
    assertEquals('', viewContainer.ariaLabel);
  });

  test('GetAriaLabelsWithIcon', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.SHIFT | Modifier.ALT | Modifier.COMMAND,
        /*key=*/ 220,
        /*keyDisplay=*/ 'LaunchApplication1');
    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();

    const viewContainer =
        viewElement.shadowRoot!.querySelector('#container') as HTMLDivElement;
    // The icon label is 'show windows'.
    const regex = /^(search|launcher) alt shift show windows$/;
    assertTrue(!!viewContainer.ariaLabel);
    assertTrue(regex.test(viewContainer.ariaLabel));
  });

  test('GetAriaLabelsWithLwinKey', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();
    // Open/close launcher -> Lwin key.
    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.NONE,
        /*key=*/ 224,
        /*keyDisplay=*/ 'Meta');
    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();

    const viewContainer =
        viewElement.shadowRoot!.querySelector('#container') as HTMLDivElement;
    const regex = /^(search|launcher)$/;
    assertTrue(!!viewContainer.ariaLabel);
    assertTrue(regex.test(viewContainer.ariaLabel));
  });

  test('CancelInputWithShortcut', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flush();

    // Assert that this is in the EDIT state.
    assertEquals(ViewState.EDIT, viewElement.viewState);

    const shortcutInput = strictQuery(
        'shortcut-input', viewElement!.shadowRoot, ShortcutInputElement);

    let ctrlKey = getCtrlElement(shortcutInput);
    let altKey = getAltElement(shortcutInput);
    let metaKey = getSearchElement(shortcutInput);
    let shiftKey = getShiftElement(shortcutInput);
    let pendingKey = getPendingKeyElement(shortcutInput);

    // By default, no keys should be registered.
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    // Simulate Alt.
    const keyEvent: KeyEvent = {
      vkey: VKey.kMenu,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'alt',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flush();

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    // Now press Escape.
    const keyEvent2: KeyEvent = {
      vkey: VKey.kEscape,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'escape',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent2, keyEvent2);

    await flush();

    ctrlKey = getCtrlElement(shortcutInput);
    altKey = getAltElement(shortcutInput);
    metaKey = getSearchElement(shortcutInput);
    shiftKey = getShiftElement(shortcutInput);
    pendingKey = getPendingKeyElement(shortcutInput);

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);

    // Expect that press Alt + Esc will cancel the edit state.
    assertEquals(ViewState.VIEW, viewElement.viewState);
    assertFalse(viewElement.hasError);
    assertEquals('', viewElement.statusMessage);
  });
});
