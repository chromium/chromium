// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeShortcutInputProvider} from 'chrome://resources/ash/common/shortcut_input_ui/fake_shortcut_input_provider.js';
import {KeyEvent} from 'chrome://resources/ash/common/shortcut_input_ui/input_device_settings.mojom-webui.js';
import {Modifier as ModifierEnum} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeDefaultAccelerators, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {setShortcutProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {setShortcutInputProviderForTesting} from 'chrome://shortcut-customization/js/shortcut_input_mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorResultData, Subactions} from 'chrome://shortcut-customization/mojom-webui/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createStandardAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorEditViewTest', function() {
  let editViewElement: AcceleratorEditViewElement|null = null;
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

    editViewElement = document.createElement('accelerator-edit-view');
    document.body.appendChild(editViewElement);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    editViewElement!.remove();
    editViewElement = null;
  });

  function getElementById(id: string): HTMLElement {
    assertTrue(!!editViewElement);
    const element = editViewElement.shadowRoot!.getElementById(id);
    assertTrue(!!element);
    return element as HTMLElement;
  }

  test('LoadsBasicEditView', async () => {
    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    await flush();

    // Check that the edit buttons are visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));

    // Verify that no metrics were sent.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on the edit button.
    getElementById('editButton')!.click();

    // Only the Cancel button should now be visible.
    assertFalse(isVisible(getElementById('editButtonsContainer')));
    assertTrue(isVisible(getElementById('cancelButtonContainer')));

    // Click on the Cancel button and expect the edit buttons to be available.
    getElementById('cancelButton')!.click();

    await flushTasks();
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));

    // Expect metric recorded.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(
        Subactions.kNoErrorCancel, provider.getLastRecordedSubactions());
  });

  test('SuccessfulEdit', async () => {
    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    await flush();

    // Check that the edit buttons are visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));

    // Verify that no metrics were sent.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on the edit button.
    getElementById('editButton')!.click();
    await flushTasks();

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Press another shortcut, expect no error.
    const keyEvent: KeyEvent = {
      vkey: VKey.kKeyE,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT | ModifierEnum.CONTROL,
      keyDisplay: 'e',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();
    assertFalse(editViewElement!.hasError);

    // Expect metric recorded.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(
        Subactions.kNoErrorSuccess, provider.getLastRecordedSubactions());
  });

  test('DetectShortcutConflict', async () => {
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    editViewElement!.source = AcceleratorSource.kAsh;
    editViewElement!.action = 1;
    await flushTasks();

    // Check that the edit buttons are visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));

    // Assert that no error has occurred.
    assertFalse(editViewElement!.hasError);
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on the edit button.
    getElementById('editButton')!.click();
    await flushTasks();

    // Press 'Snap Window left' key, expect an error since it is a
    // pre-existing shortcut.
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem4,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT | ModifierEnum.CONTROL,
      keyDisplay: 'BracketLeft',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();
    assertTrue(editViewElement!.hasError);

    const fakeResult2: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult2);
    // Press another shortcut, expect no error.
    const keyEvent2: KeyEvent = {
      vkey: VKey.kKeyE,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT | ModifierEnum.CONTROL,
      keyDisplay: 'e',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent2, keyEvent2);

    await flushTasks();
    assertFalse(editViewElement!.hasError);

    // Now click on the cancel button and expect an error.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(
        Subactions.kErrorSuccess, provider.getLastRecordedSubactions());
  });

  test('CancelError', async () => {
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    editViewElement!.source = AcceleratorSource.kAsh;
    editViewElement!.action = 1;
    await flushTasks();

    // Check that the edit buttons are visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));

    // Assert that no error has occurred.
    assertFalse(editViewElement!.hasError);
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on the edit button.
    getElementById('editButton')!.click();
    await flushTasks();

    // Press 'Snap Window left' key, expect an error since it is a
    // pre-existing shortcut.
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem4,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'BracketLeft',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();
    assertTrue(editViewElement!.hasError);

    // Click the cancel button and expect metric to be fired.
    getElementById('cancelButton')!.click();
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(Subactions.kErrorCancel, provider.getLastRecordedSubactions());
  });

  test('PressKeyToResetError', async () => {
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    editViewElement!.source = AcceleratorSource.kAsh;
    editViewElement!.action = 1;
    await flushTasks();

    // Click on the edit button.
    getElementById('editButton')!.click();

    await flushTasks();

    // Press 'Snap Window left' key, expect an error since it is a
    // pre-existing shortcut.
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem4,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'BracketLeft',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();
    assertTrue(editViewElement!.hasError);

    // Press a single key, expect error to be reset.
    const keyEvent2: KeyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent2, keyEvent2);

    await flushTasks();
    assertFalse(editViewElement!.hasError);
  });

  test('ClickEditAndShowInputHint', async () => {
    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    editViewElement!.source = AcceleratorSource.kAsh;
    editViewElement!.action = 1;
    await flushTasks();

    // Check that the edit button is visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));

    // Click on the edit button.
    getElementById('editButton')!.click();

    // Input hint message should be shown.
    const expectedHintMessage =
        'Press 1-4 modifiers and 1 other key on your keyboard. To exit ' +
        'editing mode, press alt + esc.';
    const statusMessageElement = strictQuery(
        '#container',
        editViewElement!.shadowRoot!.querySelector('#status')!.shadowRoot,
        HTMLDivElement);
    assertEquals(expectedHintMessage, statusMessageElement.textContent!.trim());
  });

  test('ClickCancelButton', async () => {
    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    await flush();

    // Click on the edit button.
    getElementById('editButton')!.click();

    // Expect cancel button is visible.
    const cancelButton = getElementById('cancelButton');
    assertTrue(isVisible(cancelButton));

    // Set up flags for event listeners
    let onBlurCalled = false;
    let onCancelButtonClickedCalled = false;

    // Attach event listeners
    editViewElement!.addEventListener('blur', () => {
      onBlurCalled = true;
    });
    cancelButton!.addEventListener('click', () => {
      onCancelButtonClickedCalled = true;
    });

    // Click on cancel button.
    cancelButton.click();
    await flush();

    // Expect blur event is not triggered.
    assertTrue(onCancelButtonClickedCalled);
    assertFalse(onBlurCalled);
  });
});
