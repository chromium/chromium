// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_edit_dialog.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeShortcutInputProvider} from 'chrome://resources/ash/common/shortcut_input_ui/fake_shortcut_input_provider.js';
import {KeyEvent} from 'chrome://resources/ash/common/shortcut_input_ui/input_device_settings.mojom-webui.js';
import {Modifier as ModifierEnum} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditDialogElement} from 'chrome://shortcut-customization/js/accelerator_edit_dialog.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeDefaultAccelerators, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {setShortcutProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {setShortcutInputProviderForTesting} from 'chrome://shortcut-customization/js/shortcut_input_mojo_interface_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorInfo, AcceleratorKeyState, AcceleratorState, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorResultData, EditDialogCompletedActions, UserAction} from 'chrome://shortcut-customization/mojom-webui/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createAliasedStandardAcceleratorInfo, createCustomStandardAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorEditDialogTest', function() {
  let viewElement: AcceleratorEditDialogElement|null = null;
  let provider: FakeShortcutProvider;
  let manager: AcceleratorLookupManager|null = null;
  const shortcutInputProvider: FakeShortcutInputProvider =
      new FakeShortcutInputProvider();

  setup(() => {
    provider = new FakeShortcutProvider();
    provider.setFakeGetDefaultAcceleratorsForId(fakeDefaultAccelerators);
    setShortcutProviderForTesting(provider);
    setShortcutInputProviderForTesting(shortcutInputProvider);
    // Set up manager.
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
    viewElement = document.createElement('accelerator-edit-dialog');
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    viewElement!.remove();
    viewElement = null;
  });

  test('LoadsBasicDialogWithCorrectOrder', async () => {
    // [ctrl + shift + g].
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    // [c].
    const acceleratorInfo3: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.NONE,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');
    // [ctrl + c].
    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2, acceleratorInfo3];

    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(3, acceleratorElements.length);
    assertEquals(
        description,
        dialog!.querySelector('#shortcutDescription')!.textContent!.trim());

    // Accelerator is sorted, the order is updated to be [c], [ctrl+c],
    // [ctrl+shift+g]
    const accelView1 =
        acceleratorElements[0]!.shadowRoot!.querySelector('accelerator-view');
    const keys1 =
        accelView1!.shadowRoot!.querySelectorAll('shortcut-input-key');
    // [c]
    assertEquals(1, keys1.length);
    assertEquals(
        'c', keys1[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    const accelView2 =
        acceleratorElements[1]!.shadowRoot!.querySelector('accelerator-view');
    const keys2 =
        accelView2!.shadowRoot!.querySelectorAll('shortcut-input-key');
    // [ctrl + c]
    assertEquals(2, keys2.length);
    assertEquals(
        'ctrl',
        keys2[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'c', keys2[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    const accelView3 =
        acceleratorElements[2]!.shadowRoot!.querySelector('accelerator-view');
    const keys3 =
        accelView3!.shadowRoot!.querySelectorAll('shortcut-input-key');
    // [ctrl + shift + g]
    assertEquals(3, keys3.length);
    assertEquals(
        'ctrl',
        keys3[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'shift',
        keys3[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys3[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    // Clicking on "Done" button will close the dialog.
    const button = dialog!.querySelector('#doneButton') as CrButtonElement;
    button.click();
    assertFalse(dialog.open);
  });

  test('AddShortcut', async () => {
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const acceleratorInfos = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = acceleratorInfos;
    viewElement!.description = description;
    await flush();
    const dialog = viewElement!.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // The "Add Shortcut" button should be visible and the pending accelerator
    // should not be visible.
    const buttonContainer =
        dialog!.querySelector('#addAcceleratorContainer') as HTMLDivElement;
    assertTrue(!!buttonContainer);
    assertFalse(buttonContainer!.hidden);
    let pendingAccelerator: AcceleratorEditViewElement|null =
        dialog!.querySelector('#pendingAccelerator');
    assertFalse(!!pendingAccelerator);

    // Clicking on the "Add Shortcut" button should hide the button and show
    // the pending shortcut.
    const addButton =
        dialog!.querySelector('#addAcceleratorButton') as CrButtonElement;
    addButton!.click();
    await flushTasks();
    assertTrue(buttonContainer!.hidden);
    // Expected the dialog's "done" button to be disabled when adding a new
    // accelerator.
    const doneButton = dialog!.querySelector('#doneButton') as CrButtonElement;
    assertTrue(doneButton!.disabled);

    // Input hint should be shown when adding a new accelerator.
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    const expectedHintMessage =
        'Press 1-4 modifiers and 1 other key on your keyboard. To exit ' +
        'editing mode, press alt + esc.';
    const statusMessageElement = strictQuery(
        '#container',
        acceleratorElements[0]!.shadowRoot!.querySelector(
                                               '#status')!.shadowRoot,
        HTMLDivElement);
    assertEquals(expectedHintMessage, statusMessageElement.textContent!.trim());

    // Re-query the stamped element.
    pendingAccelerator = dialog!.querySelector('#pendingAccelerator');
    assertTrue(!!pendingAccelerator);

    // Click on the cancel button, expect the "Add Shortcut" button to be
    // visible and the pending accelerator to be hidden.
    const cancelButton = pendingAccelerator!.shadowRoot!.querySelector(
                             '#cancelButton') as CrButtonElement;
    cancelButton.click();
    await flushTasks();

    // "done" button should now be enabled.
    assertFalse(doneButton!.disabled);

    assertFalse(buttonContainer!.hidden);
  });

  test('RestoreDefaultButtonSuccess', async () => {
    const acceleratorInfo: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const accelerators = [acceleratorInfo];

    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, acceleratorElements.length);

    // Expect call count for `restoreDefault` to be 0.
    assertEquals(0, provider.getRestoreDefaultCallCount());
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    provider.setFakeRestoreDefaultResult(fakeResult);

    await flushTasks();
    const restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    restoreDefaultButton.click();
    await flushTasks();

    // Expect call count for `restoreDefault` to be 1.
    assertEquals(1, provider.getRestoreDefaultCallCount());
    assertEquals(UserAction.kResetAction, provider.getLatestRecordedAction());

    // Click done button.
    const doneButton = dialog!.querySelector('#doneButton') as CrButtonElement;
    doneButton.click();

    // Wait until dialog is closed to make sure onDialogClose() is triggered.
    await eventToPromise('edit-dialog-closed', viewElement!);

    // Now verify last action was recorded.
    assertEquals(
        EditDialogCompletedActions.kReset,
        provider.getLastEditDialogCompletedActions());
  });

  test('RestoreDefaultButtonConflict', async () => {
    const acceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71, /*keyDisplay=*/ 'g', AcceleratorState.kDisabledByUser);

    const accelerators = [acceleratorInfo];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(0, acceleratorElements.length);

    // Expect call count for `restoreDefault` to be 0.
    assertEquals(0, provider.getRestoreDefaultCallCount());
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kRestoreSuccessWithConflicts,
      shortcutName: stringToMojoString16('TestDescription'),
    };

    provider.setFakeRestoreDefaultResult(fakeResult);

    await flushTasks();
    const restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    restoreDefaultButton.click();
    await flushTasks();

    // Expect call count for `restoreDefault` to be 1.
    assertEquals(1, provider.getRestoreDefaultCallCount());

    // Set the fake return for `GetConflictAccelerator` which is used to display
    // the error message.
    const fakeConflictResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: stringToMojoString16('TestConflictDescription'),
    };
    provider.setFakeGetConflictAccelerator(fakeConflictResult);

    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(accelerators);
    await flushTasks();

    const updatedAcceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, updatedAcceleratorElements.length);

    // Verify conflict error message is displayed.
    const expectedErrorMessage =
        'Shortcut is being used for "TestConflictDescription". Edit or ' +
        'remove to resolve the conflict.';
    const statusMessageElement = strictQuery(
        '#container',
        updatedAcceleratorElements[0]!.shadowRoot!.querySelector(
                                                      '#status')!.shadowRoot,
        HTMLDivElement);
    assertEquals(
        expectedErrorMessage, statusMessageElement.textContent!.trim());
  });

  test('RestoreDefaultButtonIgnoreConflict', async () => {
    // Set the default accelerators the same as the initialized accelerators.
    const defaultAccelerators: Accelerator[] = [{
      modifiers: Modifier.CONTROL | Modifier.SHIFT,
      keyCode: 71,
      keyState: AcceleratorKeyState.PRESSED,
    }];
    provider.setFakeGetDefaultAcceleratorsForId(defaultAccelerators);

    const acceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71, /*keyDisplay=*/ 'g', AcceleratorState.kDisabledByUser);

    const accelerators = [acceleratorInfo];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kRestoreSuccessWithConflicts,
      shortcutName: stringToMojoString16('TestDescription'),
    };

    provider.setFakeRestoreDefaultResult(fakeResult);

    await flushTasks();
    let restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    restoreDefaultButton.click();
    await flushTasks();

    // Set the fake return for `GetConflictAccelerator` which is used to display
    // the error message.
    const fakeConflictResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: stringToMojoString16('TestConflictDescription'),
    };
    provider.setFakeGetConflictAccelerator(fakeConflictResult);

    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(accelerators);
    await flushTasks();

    let updatedAcceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, updatedAcceleratorElements.length);

    // Verify that the add button and restore button are hidden.
    let addButtonContainer =
        dialog!.querySelector('#addAcceleratorContainer') as HTMLDivElement;

    restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    assertTrue(restoreDefaultButton.hidden);
    assertTrue(addButtonContainer.hidden);

    // Click on the trash button to effectively ignore the conflict.
    const cancelButton =
        updatedAcceleratorElements[0]!.shadowRoot!.querySelector(
            '#deleteButton') as HTMLButtonElement;
    cancelButton!.click();

    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(accelerators);
    await flushTasks();

    // Verify that the accelerator is now not visible.
    updatedAcceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(0, updatedAcceleratorElements.length);

    // Verify that the add button and restore button are shown.
    addButtonContainer =
        dialog!.querySelector('#addAcceleratorContainer') as HTMLDivElement;

    restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    assertFalse(restoreDefaultButton.hidden);
    assertFalse(addButtonContainer.hidden);
  });

  test('RestoreDefaultButtonFixConflict', async () => {
    const acceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71, /*keyDisplay=*/ 'g', AcceleratorState.kDisabledByUser);

    const accelerators = [acceleratorInfo];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kRestoreSuccessWithConflicts,
      shortcutName: stringToMojoString16('TestDescription'),
    };

    provider.setFakeRestoreDefaultResult(fakeResult);

    await flushTasks();
    const restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    restoreDefaultButton.click();
    await flushTasks();

    // Set the fake return for `GetConflictAccelerator` which is used to display
    // the error message.
    const fakeConflictResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: stringToMojoString16('TestConflictDescription'),
    };
    provider.setFakeGetConflictAccelerator(fakeConflictResult);

    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(accelerators);
    await flushTasks();

    let updatedAcceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, updatedAcceleratorElements.length);

    // Click on the edit button to attempt to fix the conflict.
    const editButton = updatedAcceleratorElements[0]!.shadowRoot!.querySelector(
                           '#editButton') as HTMLButtonElement;
    editButton!.click();

    await flushTasks();

    // Set the fake `AddAccelerator` mojom result.
    const fakeAddResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };
    provider.setFakeAddAcceleratorResult(fakeAddResult);

    // Expect no calls to be made to `AddAccelerator`.
    assertEquals(0, provider.getAddAcceleratorCallCount());

    // Simulate Ctrl + Alt + e.
    const keyEvent: KeyEvent = {
      vkey: VKey.kKeyE,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'e',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    // Expect one call to be made to `AddAccelerator`.
    assertEquals(1, provider.getAddAcceleratorCallCount());

    const newAcceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.ALT,
            /*key=*/ 69, /*keyDisplay=*/ 'e', AcceleratorState.kEnabled);

    const newAccelerators = [newAcceleratorInfo];
    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(newAccelerators);
    await flushTasks();

    // Verify that the accelerator is visible.
    updatedAcceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, updatedAcceleratorElements.length);
  });

  test('RestoreDefaultButtonCancelFix', async () => {
    const acceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71, /*keyDisplay=*/ 'g', AcceleratorState.kDisabledByUser);

    const accelerators = [acceleratorInfo];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flushTasks();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kRestoreSuccessWithConflicts,
      shortcutName: stringToMojoString16('TestDescription'),
    };

    provider.setFakeRestoreDefaultResult(fakeResult);
    const restoreDefaultButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    restoreDefaultButton.click();
    await flushTasks();

    // Set the fake return for `GetConflictAccelerator` which is used to display
    // the error message.
    const fakeConflictResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: stringToMojoString16('TestConflictDescription'),
    };
    provider.setFakeGetConflictAccelerator(fakeConflictResult);

    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(accelerators);
    await flushTasks();

    const updatedAcceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, updatedAcceleratorElements.length);

    // Click on the edit button to attempt to fix the conflict.
    const editButton = updatedAcceleratorElements[0]!.shadowRoot!.querySelector(
                           '#editButton') as HTMLButtonElement;
    editButton!.click();

    // Now cancel editing.
    const cancelButton =
        updatedAcceleratorElements[0]!.shadowRoot!.querySelector(
            '#cancelButton') as HTMLButtonElement;
    cancelButton!.click();

    await flushTasks();

    // Expect error message is still present.
    const expectedErrorMessage =
        'Shortcut is being used for "TestConflictDescription". Edit or ' +
        'remove to resolve the conflict.';
    const statusMessageElement = strictQuery(
        '#container',
        updatedAcceleratorElements[0]!.shadowRoot!.querySelector(
                                                      '#status')!.shadowRoot,
        HTMLDivElement);
    assertEquals(
        expectedErrorMessage, statusMessageElement.textContent!.trim());
  });

  test('FilterDisabledAccelerators', async () => {
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    // Default state is kEnabled.
    const acceleratorInfo3: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 't');

    acceleratorInfo1.state = AcceleratorState.kDisabledByUnavailableKeys;
    acceleratorInfo2.state = AcceleratorState.kDisabledByUser;

    const accelerators = [acceleratorInfo1, acceleratorInfo2, acceleratorInfo3];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');

    // Expect there are only 1 accelerator after being filtered.
    assertEquals(1, acceleratorElements.length);
  });

  test('maxReachedHintHiddenWithFewAccels', async () => {
    const acceleratorInfo: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const accelerators = [acceleratorInfo];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();

    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, acceleratorElements.length);

    // Expect maxAccelsReachedHint is hidden and addButton is visible.
    const maxAccelReachedHint =
        viewElement!.shadowRoot!.querySelector('#maxAcceleratorsReached') as
        HTMLDivElement;
    const addButtonContainer =
        viewElement!.shadowRoot!.querySelector('#addAcceleratorContainer') as
        HTMLDivElement;
    assertTrue(maxAccelReachedHint.hidden);
    assertFalse(addButtonContainer.hidden);
  });

  test('maxReachedHintVisibleWithMaxAccels', async () => {
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo3: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.COMMAND,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo4: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');
    const acceleratorInfo5: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    // Initialize with max accelerators.
    const accelerators = [
      acceleratorInfo1,
      acceleratorInfo2,
      acceleratorInfo3,
      acceleratorInfo4,
      acceleratorInfo5,
    ];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();

    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(5, acceleratorElements.length);

    // Expect maxAccelsReachedHint is visible and addButton is hidden.
    const maxAccelReachedHint =
        viewElement!.shadowRoot!.querySelector('#maxAcceleratorsReached') as
        HTMLDivElement;
    const addButtonContainer =
        viewElement!.shadowRoot!.querySelector('#addAcceleratorContainer') as
        HTMLDivElement;
    assertFalse(maxAccelReachedHint.hidden);
    assertTrue(addButtonContainer.hidden);
  });

  test('maxReachedHintNotVisibleWithDisabledAccelerator', async () => {
    const acceleratorInfo1: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71,
            /*keyDisplay=*/ 'g', AcceleratorState.kDisabledByUser);
    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo3: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.COMMAND,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo4: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');
    const acceleratorInfo5: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    // Initialize with max accelerators.
    const accelerators = [
      acceleratorInfo1,
      acceleratorInfo2,
      acceleratorInfo3,
      acceleratorInfo4,
      acceleratorInfo5,
    ];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();

    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(4, acceleratorElements.length);

    // Expect maxAccelsReachedHint is not visible and addButton is visible.
    const maxAccelReachedHint =
        viewElement!.shadowRoot!.querySelector('#maxAcceleratorsReached') as
        HTMLDivElement;
    const addButtonContainer =
        viewElement!.shadowRoot!.querySelector('#addAcceleratorContainer') as
        HTMLDivElement;
    assertTrue(maxAccelReachedHint.hidden);
    assertFalse(addButtonContainer.hidden);
  });

  test('showEmptyState', async () => {
    // Create disabled accelerator info.
    const acceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71,
            /*keyDisplay=*/ 'g', AcceleratorState.kDisabledByUser);

    viewElement!.acceleratorInfos = [acceleratorInfo];
    viewElement!.description = 'test shortcut';
    await flush();

    // Open dialog.
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);

    let noShortcutAssigned = viewElement!.shadowRoot!.querySelector(
                                 '#noShortcutAssigned') as HTMLDivElement;

    // Expect "No shortcut assigned" message is shown when there's no enabled
    // accelerators in the dialog.
    assertTrue(!!noShortcutAssigned);
    assertEquals(
        'No shortcut assigned', noShortcutAssigned.textContent!.trim());

    // Click add button, ViewState change to ADD.
    const addButton =
        dialog!.querySelector('#addAcceleratorButton') as CrButtonElement;
    assertTrue(!!addButton);
    addButton!.click();
    await flush();

    // Expect "No shortcut assigned" message is not displayed when ViewState
    // becomes ADD.
    noShortcutAssigned = viewElement!.shadowRoot!.querySelector(
                             '#noShortcutAssigned') as HTMLDivElement;
    assertFalse(!!noShortcutAssigned);
  });

  test('buttonsVisibility', async () => {
    // Set the default accelerators the same as the initialized accelerators.
    const defaultAccelerators: Accelerator[] = [{
      modifiers: Modifier.CONTROL | Modifier.SHIFT,
      keyCode: 71,
      keyState: AcceleratorKeyState.PRESSED,
    }];
    provider.setFakeGetDefaultAcceleratorsForId(defaultAccelerators);

    const acceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.SHIFT,
            /*key=*/ 71, /*keyDisplay=*/ 'g', AcceleratorState.kEnabled);

    const accelerators = [acceleratorInfo];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);

    const addButtonContainer =
        viewElement!.shadowRoot!.querySelector('#addAcceleratorContainer') as
        HTMLDivElement;
    const restoreButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    const doneButton = dialog!.querySelector('#doneButton') as CrButtonElement;

    // When first open the dialog, addButton is visible, restoreButton is
    // hidden, doneButton is not disabled.
    assertFalse(addButtonContainer.hidden);
    assertTrue(restoreButton.hidden);
    assertFalse(doneButton.disabled);

    // Click edit button, addButton is visible, restoreButton is hidden,
    // doneButton is disabled.
    const editViewElements = dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(1, editViewElements.length);
    const editButton = editViewElements[0]!.shadowRoot!.querySelector(
                           '#editButton') as HTMLButtonElement;
    editButton!.click();
    await flushTasks();
    assertFalse(addButtonContainer.hidden);
    assertTrue(restoreButton.hidden);
    assertTrue(doneButton.disabled);

    // Click cancel button, addButton is visible, restoreButton is
    // hidden, doneButton is not disabled.
    const cancelButton = editViewElements[0]!.shadowRoot!.querySelector(
                             '#cancelButton') as HTMLButtonElement;
    cancelButton!.click();
    await flushTasks();
    assertFalse(addButtonContainer.hidden);
    assertTrue(restoreButton.hidden);
    assertFalse(doneButton.disabled);

    // Click add button, addButton is hidden, restoreButton is hidden,
    // doneButton is disabled.
    const addButton =
        dialog!.querySelector('#addAcceleratorButton') as CrButtonElement;
    addButton!.click();
    await flushTasks();
    assertTrue(addButtonContainer.hidden);
    assertTrue(restoreButton.hidden);
    assertTrue(doneButton.disabled);

    // Click cancel button again.
    const pendingAccelerator = dialog!.querySelector('#pendingAccelerator');
    const cancelButton2 = pendingAccelerator!.shadowRoot!.querySelector(
                              '#cancelButton') as CrButtonElement;
    cancelButton2.click();
    await flushTasks();
    assertFalse(addButtonContainer.hidden);
    assertTrue(restoreButton.hidden);
    assertFalse(doneButton.disabled);

    // Update the accelerator, now the current accelerators are different from
    // the default accelerators.
    const newAcceleratorInfo: AcceleratorInfo =
        createCustomStandardAcceleratorInfo(
            Modifier.CONTROL | Modifier.ALT,
            /*key=*/ 69, /*keyDisplay=*/ 'e', AcceleratorState.kEnabled);
    const newAccelerators = [newAcceleratorInfo];

    // Simulate `UpdateDialogAccelerators`.
    viewElement!.updateDialogAccelerators(newAccelerators);
    await flushTasks();

    // After updating the accelerator, addButton is visible, restoreButton is
    // visible, doneButton is not disabled.
    assertFalse(addButtonContainer.hidden);
    assertFalse(restoreButton.hidden);
    assertFalse(doneButton.disabled);
  });

  test('aliasedAcceleratorNotCountedTowardsMaximumAllowed', async () => {
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo3: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.COMMAND,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');
    const acceleratorInfo4: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');
    const acceleratorInfo5: AcceleratorInfo =
        createAliasedStandardAcceleratorInfo(
            Modifier.CONTROL, /*key=*/ 67, /*keyDisplay=*/ 'c',
            AcceleratorState.kEnabled, {
              modifiers: Modifier.ALT,
              keyCode: 67,
              keyState: AcceleratorKeyState.PRESSED,
            });

    // Initialize with max accelerators.
    const accelerators = [
      acceleratorInfo1,
      acceleratorInfo2,
      acceleratorInfo3,
      acceleratorInfo4,
      acceleratorInfo5,
    ];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();

    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(5, acceleratorElements.length);

    // Expect maxAccelsReachedHint is visible and addButton is hidden.
    const maxAccelReachedHint =
        viewElement!.shadowRoot!.querySelector('#maxAcceleratorsReached') as
        HTMLDivElement;
    const addButtonContainer =
        viewElement!.shadowRoot!.querySelector('#addAcceleratorContainer') as
        HTMLDivElement;
    assertTrue(maxAccelReachedHint.hidden);
    assertFalse(addButtonContainer.hidden);
  });
});
