// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/shortcut_customization_app.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDrawerElement} from 'chrome://resources/ash/common/cr_elements/cr_drawer/cr_drawer.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeShortcutInputProvider} from 'chrome://resources/ash/common/shortcut_input_ui/fake_shortcut_input_provider.js';
import type {KeyEvent} from 'chrome://resources/ash/common/shortcut_input_ui/input_device_settings.mojom-webui.js';
import {Modifier as ModifierEnum} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import type {AcceleratorRowElement} from 'chrome://shortcut-customization/js/accelerator_row.js';
import type {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/js/accelerator_subsection.js';
import {fakeAcceleratorConfig, fakeDefaultAccelerators, fakeLayoutInfo, fakeSearchResults} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {setShortcutProviderForTesting, setUseFakeProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import type {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/js/shortcut_customization_app.js';
import {setShortcutInputProviderForTesting} from 'chrome://shortcut-customization/js/shortcut_input_mojo_interface_provider.js';
import type {LayoutInfo, MojoAcceleratorConfig, MojoLayoutInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorCategory, AcceleratorConfigResult, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutStyle, MetaKey, Modifier, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {getSubcategoryNameStringId} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import type {AcceleratorResultData} from 'chrome://shortcut-customization/mojom-webui/shortcut_customization.mojom-webui.js';
import {EditDialogCompletedActions, Subactions, UserAction} from 'chrome://shortcut-customization/mojom-webui/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

function initShortcutCustomizationAppElement():
    ShortcutCustomizationAppElement {
  const element = document.createElement('shortcut-customization-app');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('shortcutCustomizationAppTest', function() {
  let page: ShortcutCustomizationAppElement|null = null;

  let manager: AcceleratorLookupManager|null = null;

  let provider: FakeShortcutProvider;

  let handler: FakeShortcutSearchHandler;

  const shortcutInputProvider: FakeShortcutInputProvider =
      new FakeShortcutInputProvider();

  const jellyDisabledCssUrl =
      'chrome://resources/chromeos/colors/cros_styles.css';
  let linkEl: HTMLLinkElement|null = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();

    // Set up provider.
    setUseFakeProviderForTesting(true);
    provider = new FakeShortcutProvider();
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    provider.setFakeAcceleratorLayoutInfos(fakeLayoutInfo);
    provider.setFakeGetDefaultAcceleratorsForId(fakeDefaultAccelerators);
    provider.setFakeIsCustomizationAllowedByPolicy(true);
    // The meta key is displayed as the launcher key in this test.
    provider.setFakeMetaKeyToDisplay(MetaKey.kLauncher);
    provider.setHasCustomAccelerators(true);
    setShortcutProviderForTesting(provider);
    setShortcutInputProviderForTesting(shortcutInputProvider);

    // Set up SearchHandler.
    handler = new FakeShortcutSearchHandler();
    handler.setFakeSearchResult(fakeSearchResults);
    setShortcutSearchHandlerForTesting(handler);

    // Setup link element for dynamic/jelly color tests.
    linkEl = document.createElement('link');
    linkEl.href = jellyDisabledCssUrl;
    document.head.appendChild(linkEl);
  });

  teardown(() => {
    provider.reset();
    if (manager) {
      manager.reset();
    }
    if (page) {
      page.remove();
    }
    page = null;
    if (linkEl) {
      document.head.removeChild(linkEl);
    }
    linkEl = null;
  });

  function getManager(): AcceleratorLookupManager {
    assertTrue(!!manager);
    return manager;
  }

  function getPage(): ShortcutCustomizationAppElement {
    assertTrue(!!page);
    return page;
  }

  function getSubsections(category: AcceleratorCategory):
      NodeListOf<AcceleratorSubsectionElement> {
    const navPanel =
        getPage().shadowRoot!.querySelector('navigation-view-panel');
    const navBody = navPanel!.shadowRoot!.querySelector('#navigationBody');
    const subPageId = `category-${category}`;
    const subPage = navBody!.querySelector(`#${subPageId}`);
    assertTrue(!!subPage, `Expected subpage with id ${subPageId} to exist.`);
    return subPage.shadowRoot!.querySelectorAll('accelerator-subsection');
  }

  function getLinkEl(): HTMLLinkElement {
    assertTrue(!!linkEl);
    return linkEl;
  }

  async function openDialogForAcceleratorInSubsection(subsectionIndex: number) {
    // The edit dialog should not be stamped and visible.
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections(AcceleratorCategory.kWindowsAndDesks);
    const accelerators =
        subSections[subsectionIndex]!.shadowRoot!
            .querySelectorAll<AcceleratorRowElement>('accelerator-row');

    // Click on the first accelerator's edit icon, expect the edit dialog to
    // open.
    const acceleratorView =
        accelerators[0]!.shadowRoot!.querySelectorAll('accelerator-view');
    const editButton = strictQuery(
        '.edit-button', acceleratorView[0]!.shadowRoot, CrIconButtonElement);

    editButton.click();
    await flushTasks();
  }

  function triggerOnAcceleratorUpdated(): Promise<void> {
    provider.triggerOnAcceleratorUpdated();
    return flushTasks();
  }

  async function validateAcceleratorInDialog(
      acceleratorConfigResult: AcceleratorConfigResult,
      expectedErrorMessage: string) {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from second subsection.
    const dialogAccels =
        editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels.length);

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();

    await flushTasks();

    const editElement =
        editDialog.shadowRoot!.querySelector<AcceleratorEditViewElement>(
            '#pendingAccelerator');
    assertTrue(!!editElement);
    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editElement.hasError);

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: acceleratorConfigResult,
      shortcutName: 'BRIGHTNESS_UP',
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    // Dispatch an add event, this should fail as it has a failure state.
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem6,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: ']',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    assertTrue(editElement.hasError);
    assertEquals(
        expectedErrorMessage, editElement.getStatusMessageForTesting());
  }

  test('LoadFakeWindowsAndDesksPage', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    const actualSubsections =
        getSubsections(AcceleratorCategory.kWindowsAndDesks);
    const expectedLayouts =
        getManager().getSubcategories(AcceleratorCategory.kWindowsAndDesks);
    // Two subsections for this category based on the data in fake_data.ts.
    assertEquals(expectedLayouts!.size, actualSubsections.length);

    const keyIterator = expectedLayouts!.keys();
    // Assert subsection title matches expected value from fake lookup.
    const expectedFirstSubcat: AcceleratorSubcategory =
        keyIterator.next().value!;
    assertEquals(
        page.i18n(getSubcategoryNameStringId(expectedFirstSubcat)),
        actualSubsections[0]!.title);
    // Asert 2 accelerators are loaded for first subcategory.
    assertEquals(
        (expectedLayouts!.get(expectedFirstSubcat) as LayoutInfo[]).length,
        actualSubsections[0]!.accelRowDataArray.length);

    // Assert subsection title matches expected value from fake lookup.
    const expectedSecondSubcat: AcceleratorSubcategory =
        keyIterator.next().value!;
    assertEquals(
        page.i18n(getSubcategoryNameStringId(expectedSecondSubcat)),
        actualSubsections[1]!.title);
    // Assert no lock icon displayed next to subsection title under
    // WindowsAndDesks category.
    for (const subsection of actualSubsections) {
      const lockIcon = strictQuery(
          '.lock-icon-container', subsection.shadowRoot, HTMLDivElement);
      assertFalse(isVisible(lockIcon));
    }
    // Assert 2 accelerators are loaded for the second subcategory.
    assertEquals(
        (expectedLayouts!.get(expectedSecondSubcat) as LayoutInfo[]).length,
        actualSubsections[1]!.accelRowDataArray.length);
  });

  test('LoadFakeBrowserPage', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    assertEquals(undefined, provider.getLatestMainCategoryNavigated());

    const navPanel =
        getPage().shadowRoot!.querySelector('navigation-view-panel');
    const navSelector =
        navPanel!.shadowRoot!.querySelector('#navigationSelector')!
            .querySelector('navigation-selector');
    const navMenuItems =
        navSelector!.shadowRoot!.querySelector('#navigationSelectorMenu')!
            .querySelectorAll<HTMLElement>('.navigation-item');
    navMenuItems[1]!.click();

    await flushTasks();

    assertEquals(
        AcceleratorCategory.kBrowser,
        provider.getLatestMainCategoryNavigated());

    const actualSubsections = getSubsections(AcceleratorCategory.kBrowser);
    const expectedLayouts =
        getManager().getSubcategories(AcceleratorCategory.kBrowser);
    assertEquals(expectedLayouts!.size, actualSubsections.length);

    const keyIterator = expectedLayouts!.keys().next();
    // Assert subsection names match name lookup.
    assertEquals(
        page.i18n(getSubcategoryNameStringId(keyIterator.value!)),
        actualSubsections[0]!.title);
    // Assert lock icon displayed next to every subcategories under Browser
    // category.
    for (const subsection of actualSubsections) {
      const lockIcon = subsection.shadowRoot!.querySelector<IronIconElement>(
          '.lock-icon-container');
      assertTrue(isVisible(lockIcon));
    }
    // Assert only 1 accelerator is within this subsection.
    assertEquals(
        (expectedLayouts!.get(keyIterator.value!) as LayoutInfo[]).length,
        actualSubsections[0]!.accelRowDataArray.length);
  });

  test('OpenDialogFromAccelerator', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // The edit dialog should not be stamped and visible.
    let editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections(AcceleratorCategory.kWindowsAndDesks);
    const accelerators =
        subSections[0]!.shadowRoot!.querySelectorAll('accelerator-row');
    // Only three accelerators rows for this subsection.
    assertEquals(3, accelerators.length);

    // Click on the first accelerator's edit button, expect the edit dialog to
    // open.
    const acceleratorView =
        accelerators[0]!.shadowRoot!.querySelectorAll('accelerator-view');
    assertEquals(1, acceleratorView.length);
    const editButton = strictQuery(
        '.edit-button', acceleratorView[0]!.shadowRoot, CrIconButtonElement);

    editButton.click();

    await flushTasks();
    editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);
    assertEquals(
        UserAction.kOpenEditDialog, provider.getLatestRecordedAction());
  });

  test('DialogOpensOnEvent', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // The edit dialog should not be stamped and visible.
    let editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const nav = getPage().shadowRoot!.querySelector('navigation-view-panel');

    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    // Simulate the trigger event to display the dialog.
    nav!.dispatchEvent(new CustomEvent('show-edit-dialog', {
      bubbles: true,
      composed: true,
      detail: {description: 'test', accelerators: [acceleratorInfo]},
    }));
    await flushTasks();

    // Requery dialog.
    editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Click done button.
    const doneButton =
        strictQuery('#doneButton', editDialog.shadowRoot, CrButtonElement);
    doneButton.click();

    // Wait until dialog is closed to make sure onDialogClose() is triggered.
    await eventToPromise('edit-dialog-closed', editDialog);

    assertEquals(
        EditDialogCompletedActions.kNoAction,
        provider.getLastEditDialogCompletedActions());
  });

  test('ReplaceAccelerator', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from the second subsection.
    const editView =
        editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view')[0] as AcceleratorEditViewElement;

    // Click on edit button.
    editView.shadowRoot!.querySelector<CrButtonElement>('#editButton')!.click();

    await flushTasks();

    assertEquals(
        UserAction.kStartReplaceAccelerator,
        provider.getLatestRecordedAction());

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editView.hasError);

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflictCanOverride,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Alt + ']' is a conflict, expect the error message to appear.
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem6,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: ']',
    };

    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    assertTrue(editView.hasError);

    const fakeResult2: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };
    provider.setFakeReplaceAcceleratorResult(fakeResult2);

    // Press the shortcut again, this time it will replace the preexsting
    // accelerator.
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();
    const updatedEditView =
        editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view')[0] as AcceleratorEditViewElement;
    assertFalse(updatedEditView.hasError);
    assertEquals(
        UserAction.kSuccessfulModification, provider.getLatestRecordedAction());
  });

  test('AddAccelerator', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from second subsection.
    const dialogAccels =
        editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels.length);

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();

    await flushTasks();

    assertEquals(
        UserAction.kStartAddAccelerator, provider.getLatestRecordedAction());

    const editElement =
        editDialog.shadowRoot!.querySelector<AcceleratorEditViewElement>(
            '#pendingAccelerator');
    assertTrue(!!editElement);

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editElement.hasError);

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflictCanOverride,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    // Dispatch an add event, this should fail as it has a failure state.
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem6,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: ']',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    assertTrue(editElement.hasError);
    const expected_error_message =
        'Shortcut is being used for "TestConflictName". Press a new ' +
        'shortcut. To replace the existing shortcut, press this shortcut ' +
        'again.';
    assertEquals(
        expected_error_message, editElement.getStatusMessageForTesting());

    // Press a different shortcut, this time with another error state.
    const fakeResult2: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeAddAcceleratorResult(fakeResult2);

    const keyEventSpace: KeyEvent = {
      vkey: VKey.kSpace,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'space',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEventSpace, keyEventSpace);

    await flushTasks();

    const expected_error_message2 =
        'Shortcut is being used for "TestConflictName". Press a new shortcut.';
    assertEquals(
        expected_error_message2, editElement.getStatusMessageForTesting());
    assertTrue(editElement.hasError);
    // Since this was a failure, expect that latest recorded action is the same.
    assertEquals(
        UserAction.kStartAddAccelerator, provider.getLatestRecordedAction());

    // Press a different shortcut, this time with the success state.
    const fakeResult3: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };
    provider.setFakeAddAcceleratorResult(fakeResult3);

    const keyEventBrightnessUp: KeyEvent = {
      vkey: VKey.kBrightnessUp,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'BrightnessUp',
    };
    shortcutInputProvider.sendKeyPressEvent(
        keyEventBrightnessUp, keyEventBrightnessUp);

    await flushTasks();

    assertFalse(editElement.hasError);
    assertEquals(
        UserAction.kSuccessfulModification, provider.getLatestRecordedAction());

    // Click done button.
    const doneButton =
        strictQuery('#doneButton', editDialog.shadowRoot, CrButtonElement);
    doneButton.click();

    // Wait until dialog is closed to make sure onDialogClose() is triggered.
    await eventToPromise('edit-dialog-closed', editDialog);

    // Now verify last action was recorded.
    assertEquals(
        EditDialogCompletedActions.kAdd,
        provider.getLastEditDialogCompletedActions());
  });

  test('AddAcceleratorNoErrorCancel', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Expect no subactions to be recorded.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();

    await flushTasks();

    const editElement =
        editDialog.shadowRoot!.querySelector<AcceleratorEditViewElement>(
            '#pendingAccelerator');
    editElement!.shadowRoot!.querySelector<CrButtonElement>(
                                '#cancelButton')!.click();

    assertTrue(provider.getLastRecordedIsAdd());
    assertEquals(
        Subactions.kNoErrorCancel, provider.getLastRecordedSubactions());
  });

  test('AddAcceleratorErrorCancel', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Expect no subactions to be recorded.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();

    await flushTasks();

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    const editElement =
        editDialog.shadowRoot!.querySelector<AcceleratorEditViewElement>(
            '#pendingAccelerator');

    const keyEvent: KeyEvent = {
      vkey: VKey.kSpace,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'space',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    (editElement!.shadowRoot!.getElementById('cancelButton') as CrButtonElement)
        .click();

    assertTrue(provider.getLastRecordedIsAdd());
    assertEquals(Subactions.kErrorCancel, provider.getLastRecordedSubactions());
  });

  test('AddAcceleratorNoErrorSuccess', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Expect no subactions to be recorded.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();

    await flushTasks();

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    const keyEvent: KeyEvent = {
      vkey: VKey.kSpace,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'space',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    assertTrue(provider.getLastRecordedIsAdd());
    assertEquals(
        Subactions.kNoErrorSuccess, provider.getLastRecordedSubactions());
  });

  test('AddAcceleratorErrorSuccess', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Expect no subactions to be recorded.
    assertFalse(provider.getLastRecordedIsAdd());
    assertEquals(undefined, provider.getLastRecordedSubactions());

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();

    await flushTasks();

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflictCanOverride,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    const keyEvent: KeyEvent = {
      vkey: VKey.kSpace,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'space',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    // Now fix the conflict.
    const fakeResult2: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };
    provider.setFakeAddAcceleratorResult(fakeResult2);

    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    assertTrue(provider.getLastRecordedIsAdd());
    assertEquals(
        Subactions.kErrorSuccess, provider.getLastRecordedSubactions());
  });

  test('PreventDuplicateFailedRequest', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();
    await flushTasks();

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    // Before pressing any shortcut, getAddAcceleratorCallCount() should be 0.
    assertEquals(0, provider.getAddAcceleratorCallCount());

    // Press alt + ].
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem6,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'BracketRight',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    // getAddAcceleratorCallCount() should be increased to 1.
    assertEquals(1, provider.getAddAcceleratorCallCount());

    // Press alt + ] again
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    // getAddAcceleratorCallCount() should still be 1, indicating no duplicate
    // failed request has been sent to backend.
    assertEquals(1, provider.getAddAcceleratorCallCount());

    // Press another shortcut: alt + space.
    const keyEvent2: KeyEvent = {
      vkey: VKey.kSpace,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'space',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent2, keyEvent2);

    await flushTasks();

    // getAddAcceleratorCallCount() should be increased to 2.
    assertEquals(2, provider.getAddAcceleratorCallCount());
  });

  test('DuplicatedRequestCanBypass', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Click on add button.
    editDialog.shadowRoot!
        .querySelector<CrButtonElement>('#addAcceleratorButton')!.click();
    await flushTasks();

    // Set the fake mojo return call, and make the result to be
    // kConflictCanOverride.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflictCanOverride,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    // Before pressing any shortcut, getAddAcceleratorCallCount() should be 0.
    assertEquals(0, provider.getAddAcceleratorCallCount());

    // Press alt + ].
    const keyEvent: KeyEvent = {
      vkey: VKey.kOem6,
      domCode: 0,
      domKey: 0,
      modifiers: ModifierEnum.ALT,
      keyDisplay: 'BracketRight',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    // getAddAcceleratorCallCount() should be increased to 1.
    assertEquals(1, provider.getAddAcceleratorCallCount());

    // Press alt + ] again, expect it to bypass the error.
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);

    await flushTasks();

    // getAddAcceleratorCallCount() should be increased to 2.
    assertEquals(2, provider.getAddAcceleratorCallCount());
  });

  test('ValidateAcceleratorMaximumAccelerators', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kMaximumAcceleratorsReached;
    const expectedErrorMessage =
        'You can only customize 5 shortcuts. Delete a shortcut to add a new ' +
        'one.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorShiftOnlyNotAllowed', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kShiftOnlyNotAllowed;
    const expectedErrorMessage =
        'Shortcut not available. Press a new shortcut using shift and 1 ' +
        'more modifier key (ctrl, alt, or launcher).';

    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorMissingAccelerator', async () => {
    const acceleratorConfigResult = AcceleratorConfigResult.kMissingModifier;
    const expectedErrorMessage =
        'Shortcut not available. Press a new shortcut using a modifier key ' +
        '(ctrl, alt, shift, or launcher).';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorKeyNotAllowed', async () => {
    const acceleratorConfigResult = AcceleratorConfigResult.kKeyNotAllowed;
    const expectedErrorMessage =
        'Shortcut with top row keys need to include the launcher key.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorConflict', async () => {
    const acceleratorConfigResult = AcceleratorConfigResult.kConflict;
    const expectedErrorMessage =
        'Shortcut is being used for "BRIGHTNESS_UP". Press a new shortcut.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorConflictCanOverride', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kConflictCanOverride;
    const expectedErrorMessage =
        'Shortcut is being used for "BRIGHTNESS_UP". Press a new shortcut. ' +
        'To replace the existing shortcut, press this shortcut again.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateNonStandardWithSearch', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kNonStandardWithSearch;
    const expectedErrorMessage =
        '] is not available with the launcher key. Press a new shortcut.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('DisableDefaultAccelerator', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from second subsection.
    const dialogAccels =
        editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels.length);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: 'TestConflictName',
    };
    provider.setFakeRemoveAcceleratorResult(fakeResult);

    // Expect call count for `restoreDefault` to be 0.
    assertEquals(0, provider.getRemoveAcceleratorCallCount());

    // Click on remove button.
    const editView = dialogAccels[0] as AcceleratorEditViewElement;
    const deleteButton =
        editView.shadowRoot!.querySelector<CrButtonElement>('#deleteButton');
    assertTrue(!!deleteButton);
    deleteButton.click();

    await flushTasks();

    // Expect call count for `restoreDefault` to be 1.
    assertEquals(1, provider.getRemoveAcceleratorCallCount());

    assertEquals(
        UserAction.kRemoveAccelerator, provider.getLatestRecordedAction());
  });

  test('RestoreAllButtonShownWithFlag', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    page = initShortcutCustomizationAppElement();
    waitAfterNextRender(getPage());
    await flushTasks();
    const restoreButton =
        getPage()
            .shadowRoot!.querySelector('shortcuts-bottom-nav-content')!
            .shadowRoot!.querySelector<CrButtonElement>('#restoreAllButton');
    await flushTasks();
    assertTrue(isVisible(restoreButton));
  });

  test('RestoreAllButtonHiddenWithoutFlag', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: false});
    page = initShortcutCustomizationAppElement();
    waitAfterNextRender(getPage());
    await flushTasks();
    const restoreButton =
        getPage()
            .shadowRoot!.querySelector('shortcuts-bottom-nav-content')!
            .shadowRoot!.querySelector<CrButtonElement>('#restoreAllButton');
    await flushTasks();
    assertFalse(isVisible(restoreButton));
  });

  test('CurrentPageChangesWhenURLIsUpdated', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: false});
    page = initShortcutCustomizationAppElement();
    waitAfterNextRender(getPage());
    await flushTasks();

    // At first, the selected page should be the first page.
    // For the fake data, Windows & Desks is the first page.
    assertEquals(
        `category-${AcceleratorCategory.kWindowsAndDesks}`,
        page.$.navigationPanel.selectedItem.id);

    // Notify the app that the route has changed, and the selected page should
    // change too.
    let url = new URL('chrome://shortcut-customization');
    url.searchParams.append('action', '0');
    url.searchParams.append(
        'category', AcceleratorCategory.kBrowser.toString());
    page.onRouteChanged(url);
    await flushTasks();
    assertEquals(
        `category-${AcceleratorCategory.kBrowser}`,
        page.$.navigationPanel.selectedItem.id);

    // If we notify with a URL that doesn't contain the correct params, the
    // selected page should not change.
    url = new URL('chrome://shortcut-customization');
    page.onRouteChanged(url);
    await flushTasks();
    assertEquals(
        `category-${AcceleratorCategory.kBrowser}`,
        page.$.navigationPanel.selectedItem.id);

    // If we notify with a URL that contains extra params, the selected page
    // should change.
    url = new URL('chrome://shortcut-customization');
    url.searchParams.append('action', '0');
    url.searchParams.append(
        'category', AcceleratorCategory.kWindowsAndDesks.toString());
    url.searchParams.append('fake-param', 'fake-value');
    page.onRouteChanged(url);
    await flushTasks();
    assertEquals(
        `category-${AcceleratorCategory.kWindowsAndDesks}`,
        page.$.navigationPanel.selectedItem.id);
  });

  test('IsJellyEnabledForShortcutCustomization_DisabledKeepsCSS', async () => {
    loadTimeData.overrideValues({
      isJellyEnabledForShortcutCustomization: false,
    });

    page = initShortcutCustomizationAppElement();
    await flushTasks();

    assertTrue(getLinkEl().href.includes(jellyDisabledCssUrl));
  });

  test('IsJellyEnabledForShortcutCustomization_EnabledUpdatesCSS', async () => {
    loadTimeData.overrideValues({
      isJellyEnabledForShortcutCustomization: true,
    });
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    assertTrue(getLinkEl().href.includes('chrome://theme/colors.css'));
  });

  test('TextAcceleratorLookupUpdatesCorrectly', async () => {
    // Set up test to only have one shortcut.
    const testLayoutInfo: MojoLayoutInfo[] = [
      {
        category: AcceleratorCategory.kWindowsAndDesks,
        subCategory: AcceleratorSubcategory.kWindows,
        description: 'Go to windows 1 through 8',
        style: LayoutStyle.kText,
        source: AcceleratorSource.kAmbient,
        action: 1,
      },
    ];
    provider.setFakeAcceleratorLayoutInfos(testLayoutInfo);

    page = initShortcutCustomizationAppElement();
    waitAfterNextRender(getPage());
    await flushTasks();

    // This config is constructed to match the generated mojo type for an
    // accelerator configuration. `layoutProperties` is an union type, so
    // we do not have an undefined `standardAccelerator`.
    const testAcceleratorConfig: MojoAcceleratorConfig = {
      [AcceleratorSource.kAmbient]: {
        [1]: [{
          type: AcceleratorType.kDefault,
          state: AcceleratorState.kEnabled,
          acceleratorLocked: false,
          locked: true,
          layoutProperties: {
            textAccelerator: {
              parts: [
                {
                  text: 'ctrl',
                  type: TextAcceleratorPartType.kModifier,
                },
                {
                  text: ' + ',
                  type: TextAcceleratorPartType.kDelimiter,
                },
                {
                  text: '1 ',
                  type: TextAcceleratorPartType.kKey,
                },
                {
                  text: 'through ',
                  type: TextAcceleratorPartType.kPlainText,
                },
                {
                  text: '8',
                  type: TextAcceleratorPartType.kKey,
                },
              ],
            },
          },
        }],
      },
    };

    // Cycle tabs accelerator from kAmbient[1].
    const expectedCycleTabsAction = 1;

    let textLookup = getManager().getTextAcceleratorInfos(
        AcceleratorSource.kAmbient, expectedCycleTabsAction);
    assertEquals(1, textLookup.length);

    // Now simulate an update.
    provider.setFakeAcceleratorsUpdated([testAcceleratorConfig]);
    provider.setFakeMetaKeyToDisplay(MetaKey.kLauncher);
    await triggerOnAcceleratorUpdated();
    await provider.getAcceleratorsUpdatedPromiseForTesting();

    // After a call to `onAcceleratorsUpdated` we should still expect to have
    // one text accelerator.
    textLookup = getManager().getTextAcceleratorInfos(
        AcceleratorSource.kAmbient, expectedCycleTabsAction);
    assertEquals(1, textLookup.length);
  });

  test('DialogAcceleratorUpdateOnAcceleartorsUpdated', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});

    // Set up test to only have one shortcut.
    // Category: Windows and Desks, Subcategory: Desks.
    // Shortcut: Create Desk - [search shift +].
    const testLayoutInfo: MojoLayoutInfo[] = [
      {
        category: AcceleratorCategory.kWindowsAndDesks,
        subCategory: AcceleratorSubcategory.kDesks,
        description: 'Create Desk',
        style: LayoutStyle.kDefault,
        source: AcceleratorSource.kAsh,
        action: 2,
      },
    ];

    const testAcceleratorConfig: MojoAcceleratorConfig = {
      [AcceleratorSource.kAsh]: {
        [2]: [{
          type: AcceleratorType.kDefault,
          state: AcceleratorState.kEnabled,
          acceleratorLocked: false,
          locked: false,
          layoutProperties: {
            standardAccelerator: {
              keyDisplay: '+',
              accelerator: {
                modifiers: Modifier.COMMAND | Modifier.SHIFT,
                keyCode: 187,
                keyState: 0,
                timeStamp: {
                  internalValue: BigInt(0),
                },
              },
              originalAccelerator: null,
            },
          },
        }],
      },
    };

    provider.setFakeAcceleratorLayoutInfos(testLayoutInfo);
    provider.setFakeAcceleratorConfig(testAcceleratorConfig);
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for the shortcut.
    await openDialogForAcceleratorInSubsection(0);

    let editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);
    const dialogAccels =
        editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');

    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels.length);

    // Update shortcut: Create Desk - [search shift +] and [search a].
    const testUpdatedAcceleratorConfig: MojoAcceleratorConfig = {
      [AcceleratorSource.kAsh]: {
        [2]: [
          {
            type: AcceleratorType.kDefault,
            state: AcceleratorState.kEnabled,
            acceleratorLocked: false,
            locked: false,
            layoutProperties: {
              standardAccelerator: {
                keyDisplay: '+',
                accelerator: {
                  modifiers: Modifier.COMMAND | Modifier.SHIFT,
                  keyCode: 187,
                  keyState: 0,
                  timeStamp: {
                    internalValue: BigInt(0),
                  },
                },
                originalAccelerator: null,
              },
            },
          },
          {
            type: AcceleratorType.kDefault,
            state: AcceleratorState.kEnabled,
            acceleratorLocked: false,
            locked: false,
            layoutProperties: {
              standardAccelerator: {
                keyDisplay: 'a',
                accelerator: {
                  modifiers: Modifier.COMMAND,
                  keyCode: 65,
                  keyState: 0,
                  timeStamp: {
                    internalValue: BigInt(0),
                  },
                },
                originalAccelerator: null,
              },
            },
          },
        ],
      },
    };

    const simulateAcceleratorUpdate = async (
        acceleratorUpdateInProgress: boolean, expectedLength: number) => {
      // Set acceleratorUpdateInProgress.
      page!.setAcceleratorUpdateInProgressForTesting(
          acceleratorUpdateInProgress);
      // Simulate an update.
      provider.setFakeAcceleratorsUpdated([testUpdatedAcceleratorConfig]);
      provider.setFakeMetaKeyToDisplay(MetaKey.kLauncher);
      await triggerOnAcceleratorUpdated();
      await provider.getAcceleratorsUpdatedPromiseForTesting();

      // Dialog should be still open.
      editDialog = getPage().shadowRoot!.querySelector('#editDialog');
      assertTrue(!!editDialog);
      // Verify the number of dialog accelerators is expected.
      const updatedDialogAccels =
          editDialog.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
              'accelerator-edit-view');
      assertEquals(expectedLength, updatedDialogAccels.length);
    };

    // Set acceleratorUpdateInProgress to true, dialog should not be updated.
    await simulateAcceleratorUpdate(
        /*acceleratorUpdateInProgress=*/ true, /*expectedLength*/ 1);

    // Set acceleratorUpdateInProgress to true, dialog should be updated and the
    // number of dialog accelerators should be increased to 2.
    await simulateAcceleratorUpdate(
        /*acceleratorUpdateInProgress=*/ false, /*expectedLength*/ 2);
  });

  test('BottomNavContentPresentInSideNav', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const navigationPanel =
        strictQuery('navigation-view-panel', getPage().shadowRoot, HTMLElement);
    const sideNav =
        strictQuery('#sideNav', navigationPanel.shadowRoot, HTMLDivElement);
    const navContentInSideNavSlot = sideNav.querySelector<HTMLSlotElement>(
        'slot[name=bottom-nav-content-panel]');
    assertTrue(!!navContentInSideNavSlot);
    const navContentInSideNavWrapper =
        navContentInSideNavSlot.assignedElements()[0];
    assertTrue(!!navContentInSideNavWrapper);
    const navContentInSideNav = navContentInSideNavWrapper.querySelector(
        'shortcuts-bottom-nav-content');
    assertTrue(
        !!navContentInSideNav, 'Bottom nav content in side nav should exist');
  });

  test('BottomNavContentPresentInDrawer', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const navigationPanel =
        strictQuery('navigation-view-panel', getPage().shadowRoot, HTMLElement);
    const drawer =
        strictQuery('cr-drawer', navigationPanel.shadowRoot, CrDrawerElement);
    const navContentInDrawerSlot = drawer.querySelector<HTMLSlotElement>(
        'slot[name=bottom-nav-content-drawer]');
    assertTrue(!!navContentInDrawerSlot);
    const navContentInDrawerWrapper =
        navContentInDrawerSlot?.assignedElements()[0];
    assertTrue(!!navContentInDrawerWrapper);
    const navContentInDrawer =
        navContentInDrawerWrapper.querySelector('shortcuts-bottom-nav-content');
    assertTrue(
        !!navContentInDrawer, 'Bottom nav content in drawer should exist');
  });

  test('LaunchKeyboardSettings', async () => {
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const actualLink =
        getPage()
            .shadowRoot!.querySelector('shortcuts-bottom-nav-content')!
            .shadowRoot!.querySelector('#keyboardSettingsLinkContainer')!
            .querySelector<HTMLLinkElement>('#keyboardSettingsLink');
    assertTrue(!!actualLink);
    assertEquals('chrome://os-settings/per-device-keyboard', actualLink.href);
  });

  test('PolicyIndicatorShown', async () => {
    provider.setFakeIsCustomizationAllowedByPolicy(false);
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const policyIndicator =
        getPage().shadowRoot!.querySelector<HTMLElement>('#policyIndicator');
    assertTrue(!!policyIndicator);
  });

  test('PolicyIndicatorHidden', async () => {
    provider.setFakeIsCustomizationAllowedByPolicy(true);
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const policyIndicator =
        getPage().shadowRoot!.querySelector<HTMLElement>('#policyIndicator');
    assertFalse(!!policyIndicator);
  });
});
