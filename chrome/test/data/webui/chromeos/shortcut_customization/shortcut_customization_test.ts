// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/shortcut_customization_app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/js/accelerator_row.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/js/accelerator_subsection.js';
import {AcceleratorViewElement} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeAcceleratorConfig, fakeLayoutInfo, fakeSearchResults} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {setShortcutProviderForTesting, setUseFakeProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/js/shortcut_customization_app.js';
import {AcceleratorCategory, AcceleratorConfig, AcceleratorConfigResult, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutInfo, LayoutStyle, Modifier, MojoLayoutInfo, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {getSubcategoryNameStringId} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {AcceleratorResultData} from 'chrome://shortcut-customization/mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

// Converts a JS string to mojo_base::mojom::String16 object.
function strToMojoString16(str: string): {data: number[]} {
  const arr = [];
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

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
    provider.setFakeHasLauncherButton(true);

    setShortcutProviderForTesting(provider);

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
    return manager as AcceleratorLookupManager;
  }

  function getPage(): ShortcutCustomizationAppElement {
    assertTrue(!!page);
    return page as ShortcutCustomizationAppElement;
  }

  function getDialog(selector: string) {
    return getPage().shadowRoot!.querySelector(selector) as CrDialogElement;
  }

  function getSubsections(category: AcceleratorCategory):
      NodeListOf<AcceleratorSubsectionElement> {
    const navPanel =
        getPage().shadowRoot!.querySelector('navigation-view-panel');
    const navBody = navPanel!!.shadowRoot!.querySelector('#navigationBody');
    const subPageId = `category-${category}`;
    const subPage = navBody!.querySelector(`#${subPageId}`);
    assertTrue(!!subPage, `Expected subpage with id ${subPageId} to exist.`);
    return subPage!.shadowRoot!.querySelectorAll('accelerator-subsection');
  }

  function getLinkEl(): HTMLLinkElement {
    assertTrue(!!linkEl);
    return linkEl as HTMLLinkElement;
  }

  async function openDialogForAcceleratorInSubsection(subsectionIndex: number) {
    // The edit dialog should not be stamped and visible.
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections(AcceleratorCategory.kWindowsAndDesks);
    const accelerators =
        subSections[subsectionIndex]!.shadowRoot!.querySelectorAll(
            'accelerator-row') as NodeListOf<AcceleratorRowElement>;

    // Click on the first accelerator's edit icon, expect the edit dialog to
    // open.
    const acceleratorView =
        accelerators[0]!.shadowRoot!.querySelectorAll('accelerator-view');
    const editIconContainer = acceleratorView[0]!.shadowRoot!.querySelector(
                                  '.edit-icon-container') as HTMLDivElement;
    editIconContainer.click();
    await flushTasks();
  }

  function triggerOnAcceleratorUpdated(): Promise<void> {
    provider.triggerOnAcceleratorUpdated();
    return flushTasks();
  }

  async function validateAcceleratorInDialog(
      acceleratorConfigResult: AcceleratorConfigResult,
      expectedErrorMessage: string) {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from second subsection.
    const dialogAccels =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels!.length);

    // Click on add button.
    (editDialog!.shadowRoot!.querySelector('#addAcceleratorButton') as
     CrButtonElement)
        .click();

    await flushTasks();

    const editElement =
        editDialog!.shadowRoot!.querySelector('#pendingAccelerator') as
        AcceleratorEditViewElement;
    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editElement.hasError);

    const viewElement =
        editElement!.shadowRoot!.querySelector('#acceleratorItem');

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: acceleratorConfigResult,
      shortcutName: stringToMojoString16('BRIGHTNESS_UP'),
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    // press alt + ].
    const fakeKeyboardEvent = new KeyboardEvent('keydown', {
      key: ']',
      keyCode: 221,
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    });

    // Dispatch an add event, this should fail as it has a failure state.
    viewElement!.dispatchEvent(fakeKeyboardEvent);
    await flushTasks();

    assertTrue(editElement.hasError);
    assertEquals(
        expectedErrorMessage, editElement.getStatusMessageForTesting());
  }

  test('LoadFakeWindowsAndDesksPage', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    const actualSubsections =
        getSubsections(AcceleratorCategory.kWindowsAndDesks);
    const expectedLayouts =
        getManager().getSubcategories(AcceleratorCategory.kWindowsAndDesks);
    // Two subsections for this category based on the data in fake_data.ts.
    assertEquals(expectedLayouts!.size, actualSubsections!.length);

    const keyIterator = expectedLayouts!.keys();
    // Assert subsection title matches expected value from fake lookup.
    const expectedFirstSubcat: AcceleratorSubcategory =
        keyIterator.next().value;
    assertEquals(
        page.i18n(getSubcategoryNameStringId(expectedFirstSubcat)),
        actualSubsections[0]!.title);
    // Asert 2 accelerators are loaded for first subcategory.
    assertEquals(
        (expectedLayouts!.get(expectedFirstSubcat) as LayoutInfo[]).length,
        actualSubsections[0]!.accelRowDataArray!.length);

    // Assert subsection title matches expected value from fake lookup.
    const expectedSecondSubcat: AcceleratorSubcategory =
        keyIterator.next().value;
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
        actualSubsections[1]!.accelRowDataArray!.length);
  });

  test('LoadFakeBrowserPage', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    const navPanel =
        getPage().shadowRoot!.querySelector('navigation-view-panel');
    const navSelector =
        navPanel!.shadowRoot!.querySelector('#navigationSelector')!
            .querySelector('navigation-selector');
    const navMenuItems =
        navSelector!.shadowRoot!.querySelector('#navigationSelectorMenu')!
            .querySelectorAll('.navigation-item') as NodeListOf<HTMLDivElement>;
    navMenuItems[1]!.click();

    await flushTasks();

    const actualSubsections = getSubsections(AcceleratorCategory.kBrowser);
    const expectedLayouts =
        getManager().getSubcategories(AcceleratorCategory.kBrowser);
    assertEquals(expectedLayouts!.size, actualSubsections!.length);

    const keyIterator = expectedLayouts!.keys().next();
    // Assert subsection names match name lookup.
    assertEquals(
        page.i18n(getSubcategoryNameStringId(keyIterator.value)),
        actualSubsections[0]!.title);
    // Assert lock icon displayed next to every subcategories under Browser
    // category.
    for (const subsection of actualSubsections) {
      const lockIcon = subsection!.shadowRoot!.querySelector(
                           '.lock-icon-container') as IronIconElement;
      assertTrue(isVisible(lockIcon));
    }
    // Assert only 1 accelerator is within this subsection.
    assertEquals(
        (expectedLayouts!.get(keyIterator.value) as LayoutInfo[]).length,
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
    const editIconContainer = acceleratorView[0]!.shadowRoot!.querySelector(
                                  '.edit-icon-container') as HTMLDivElement;
    editIconContainer.click();

    await flushTasks();
    editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);
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

    // Close the dialog.
    const dialog =
        editDialog!.shadowRoot!.querySelector('#editDialog') as CrDialogElement;
    dialog.close();
    await flushTasks();

    assertFalse(dialog.open);
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
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view')[0] as AcceleratorEditViewElement;

    // Click on edit button.
    (editView!.shadowRoot!.querySelector('#editButton') as CrButtonElement)
        .click();

    await flushTasks();

    const accelViewElement =
        editView.shadowRoot!.querySelector('#acceleratorItem');

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editView.hasError);

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflictCanOverride,
      shortcutName: strToMojoString16('TestConflictName'),
    };
    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Alt + ']' is a conflict, expect the error message to appear.
    accelViewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: 221,
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();

    assertTrue(editView.hasError);

    const fakeResult2: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: undefined,
    };
    provider.setFakeReplaceAcceleratorResult(fakeResult2);

    // Press the shortcut again, this time it will replace the preexsting
    // accelerator.
    accelViewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: 221,
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();
    const updatedEditView =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view')[0] as AcceleratorEditViewElement;
    assertFalse(updatedEditView.hasError);
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
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels!.length);

    // Click on add button.
    (editDialog!.shadowRoot!.querySelector('#addAcceleratorButton') as
     CrButtonElement)
        .click();

    await flushTasks();

    const editElement =
        editDialog!.shadowRoot!.querySelector('#pendingAccelerator') as
        AcceleratorEditViewElement;

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editElement.hasError);

    const viewElement =
        editElement!.shadowRoot!.querySelector('#acceleratorItem');

    // Set the fake mojo return call.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflictCanOverride,
      shortcutName: strToMojoString16('TestConflictName'),
    };
    provider.setFakeAddAcceleratorResult(fakeResult);

    // Dispatch an add event, this should fail as it has a failure state.
    viewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: 221,
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();

    assertTrue(editElement.hasError);
    const expected_error_message =
        'Shortcut is used by TestConflictName. Press a new shortcut or press ' +
        'the same one again to use it for this action instead.';

    assertEquals(
        expected_error_message,
        editElement!.shadowRoot!.querySelector('#acceleratorInfoText')!
            .textContent!.trim());

    // Press the shortcut again, this time with another error state.
    const fakeResult2: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: strToMojoString16('TestConflictName'),
    };
    provider.setFakeAddAcceleratorResult(fakeResult2);

    viewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: 221,
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();

    const expected_error_message2 =
        'Shortcut is used by TestConflictName. Press a new shortcut to ' +
        'replace.';

    assertEquals(
        expected_error_message2,
        editElement!.shadowRoot!.querySelector('#acceleratorInfoText')!
            .textContent!.trim());
    assertTrue(editElement.hasError);

    // Press the shortcut again, this time with another success state.
    const fakeResult3: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: undefined,
    };
    provider.setFakeAddAcceleratorResult(fakeResult3);

    viewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: 221,
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();

    assertFalse(editElement.hasError);
  });

  test('ValidateAcceleratorMaximumAccelerators', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kMaximumAcceleratorsReached;
    const expectedErrorMessage = 'Maximum accelerators have reached.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorShiftOnlyNotAllowed', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kShiftOnlyNotAllowed;
    const expectedErrorMessage =
        'Shortcut is not valid. Shift can not be used as the only modifier ' +
        'key. Press a new shortcut.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorMissingAccelerator', async () => {
    const acceleratorConfigResult = AcceleratorConfigResult.kMissingModifier;
    const expectedErrorMessage =
        'Shortcut is not valid. Must include at lease one modifier key. ' +
        'Press a new shortcut.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorKeyNotAllowed', async () => {
    const acceleratorConfigResult = AcceleratorConfigResult.kKeyNotAllowed;
    const expectedErrorMessage =
        'Shortcut with top row keys need to include the search key.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorConflict', async () => {
    const acceleratorConfigResult = AcceleratorConfigResult.kConflict;
    const expectedErrorMessage =
        'Shortcut is used by BRIGHTNESS_UP. Press a new shortcut to replace.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('ValidateAcceleratorConflictCanOverride', async () => {
    const acceleratorConfigResult =
        AcceleratorConfigResult.kConflictCanOverride;
    const expectedErrorMessage =
        'Shortcut is used by BRIGHTNESS_UP. Press a new shortcut or press ' +
        'the same one again to use it for this action instead.';
    await validateAcceleratorInDialog(
        acceleratorConfigResult, expectedErrorMessage);
  });

  test('DisableDefaultAccelerator', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    manager!.setAcceleratorLookup(fakeAcceleratorConfig);
    manager!.setAcceleratorLayoutLookup(fakeLayoutInfo);
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    // Open dialog for first accelerator in second subsection.
    await openDialogForAcceleratorInSubsection(1);
    const editDialog = getDialog('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from second subsection.
    let acceleratorList =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');

    // Get the accelerator info before removal.
    const accelViewElement = strictQuery(
        '#acceleratorItem', acceleratorList[0]!.shadowRoot,
        AcceleratorViewElement);
    const acceleratorInfo =
        (accelViewElement as AcceleratorViewElement).acceleratorInfo;

    // Before removal, there should be exactly one accelerator present in the
    // dialog, and its state should be set to kEnabled.
    assertEquals(1, acceleratorList!.length);
    assertEquals(AcceleratorState.kEnabled, acceleratorInfo.state);

    // Click on remove button.
    const editView = acceleratorList[0] as AcceleratorEditViewElement;
    const deleteButton =
        editView!.shadowRoot!.querySelector('#deleteButton') as CrButtonElement;
    deleteButton.click();

    await flushTasks();

    // Requery the accelerator elements.
    acceleratorList =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');

    // After removal, expect that the accelerator has now been disabled and
    // removed.
    acceleratorList =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    assertEquals(0, acceleratorList!.length);
    assertEquals(AcceleratorState.kDisabledByUser, acceleratorInfo.state);
  });

  test('RestoreAllButton', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    page = initShortcutCustomizationAppElement();
    await flushTasks();

    let restoreDialog = getDialog('#restoreDialog');
    // Expect the dialog to not appear initially.
    assertFalse(!!restoreDialog);

    // Click on the Restore all button.
    const restoreButton =
        getPage()
            .shadowRoot!.querySelector('shortcuts-bottom-nav-content')!
            .shadowRoot!.querySelector('#restoreAllButton') as CrButtonElement;
    restoreButton!.click();

    await flushTasks();

    // Requery the dialog.
    restoreDialog = getDialog('#restoreDialog');
    assertTrue(restoreDialog!.open);

    const confirmButton =
        restoreDialog!.querySelector('#confirmButton') as CrButtonElement;
    confirmButton.click();

    await flushTasks();

    // Confirm dialog is now closed.
    restoreDialog = getDialog('#restoreDialog');
    assertFalse(!!restoreDialog);

    // Re-open the Restore All dialog.
    restoreButton!.click();
    await flushTasks();

    restoreDialog = getDialog('#restoreDialog');
    assertTrue(restoreDialog!.open);

    // Click on Cancel button.
    const cancelButton =
        restoreDialog!.querySelector('#cancelButton') as CrButtonElement;
    cancelButton.click();

    await flushTasks();

    restoreDialog = getDialog('#restoreDialog');
    assertFalse(!!restoreDialog);
  });

  test('RestoreAllButtonShownWithFlag', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    page = initShortcutCustomizationAppElement();
    waitAfterNextRender(getPage());
    await flushTasks();
    const restoreButton =
        getPage()
            .shadowRoot!.querySelector('shortcuts-bottom-nav-content')!
            .shadowRoot!.querySelector('#restoreAllButton') as CrButtonElement;
    await flushTasks();
    assertTrue(isVisible(restoreButton));
  });

  test('RestoreAllButtonHiddenWithoutFlag', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    page = initShortcutCustomizationAppElement();
    waitAfterNextRender(getPage());
    await flushTasks();
    const restoreButton =
        getPage()
            .shadowRoot!.querySelector('shortcuts-bottom-nav-content')!
            .shadowRoot!.querySelector('#restoreAllButton') as CrButtonElement;
    await flushTasks();
    assertFalse(isVisible(restoreButton));
  });

  test('CurrentPageChangesWhenURLIsUpdated', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
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
        description: strToMojoString16('Go to windows 1 through 8'),
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
    const testAcceleratorConfig: AcceleratorConfig = {
      [AcceleratorSource.kAmbient]: {
        [1]: [{
          type: AcceleratorType.kDefault,
          state: AcceleratorState.kEnabled,
          locked: true,
          layoutProperties: {
            textAccelerator: {
              parts: [
                {
                  text: strToMojoString16('ctrl'),
                  type: TextAcceleratorPartType.kModifier,
                },
                {
                  text: strToMojoString16(' + '),
                  type: TextAcceleratorPartType.kDelimiter,
                },
                {
                  text: strToMojoString16('1 '),
                  type: TextAcceleratorPartType.kKey,
                },
                {
                  text: strToMojoString16('through '),
                  type: TextAcceleratorPartType.kPlainText,
                },
                {
                  text: strToMojoString16('8'),
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
    provider.setFakeHasLauncherButton(true);
    await triggerOnAcceleratorUpdated();
    await provider.getAcceleratorsUpdatedPromiseForTesting();

    // After a call to `onAcceleratorsUpdated` we should still expect to have
    // one text accelerator.
    textLookup = getManager().getTextAcceleratorInfos(
        AcceleratorSource.kAmbient, expectedCycleTabsAction);
    assertEquals(1, textLookup.length);
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

  test('LaunchOldKeyboardSettings', async () => {
    loadTimeData.overrideValues({
      isInputDeviceSettingsSplitEnabled: false,
    });
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const actualLink =
        getPage()
            .shadowRoot!.querySelector(
                            'shortcuts-bottom-nav-content')!.shadowRoot!
            .querySelector('#keyboardSettingsLinkContainer')!.querySelector(
                '#keyboardSettingsLink') as HTMLLinkElement;
    assertEquals('chrome://os-settings/keyboard-overlay', actualLink.href);
  });

  test('LaunchNewKeyboardSettings', async () => {
    loadTimeData.overrideValues({
      isInputDeviceSettingsSplitEnabled: true,
    });
    page = initShortcutCustomizationAppElement();
    await flushTasks();
    const actualLink =
        getPage()
            .shadowRoot!.querySelector(
                            'shortcuts-bottom-nav-content')!.shadowRoot!
            .querySelector('#keyboardSettingsLinkContainer')!.querySelector(
                '#keyboardSettingsLink') as HTMLLinkElement;
    assertEquals('chrome://os-settings/per-device-keyboard', actualLink.href);
  });
});
