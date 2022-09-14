// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/shortcut_customization_app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/js/accelerator_row.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/js/accelerator_subsection.js';
import {AcceleratorViewElement} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeSubCategories} from 'chrome://shortcut-customization/js/fake_data.js';
import {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/js/shortcut_customization_app.js';
import {AcceleratorInfo, AcceleratorKeys, LayoutInfoList, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {createUserAccelerator} from './shortcut_customization_test_util.js';

suite('shortcutCustomizationAppTest', function() {
  let page: ShortcutCustomizationAppElement|null = null;

  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();

    page = document.createElement('shortcut-customization-app');
    document.body.appendChild(page);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    if (page) {
      page.remove();
    }
    page = null;
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

  function getSubsections(subpageId: string):
      NodeListOf<AcceleratorSubsectionElement> {
    const navPanel =
        getPage().shadowRoot!.querySelector('navigation-view-panel');
    const navBody = navPanel!!.shadowRoot!.querySelector('#navigationBody');
    const subPage = navBody!.querySelector(`#${subpageId}`);
    return subPage!.shadowRoot!.querySelectorAll('accelerator-subsection');
  }

  async function openDialogForAcceleratorInSubsection(subsectionIndex: number) {
    // The edit dialog should not be stamped and visible.
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections('chromeos-page-id');
    const accelerators =
        subSections[subsectionIndex]!.shadowRoot!.querySelectorAll(
            'accelerator-row') as NodeListOf<AcceleratorRowElement>;

    // Click on the first accelerator, expect the edit dialog to open.
    accelerators[0]!.click();
    await flushTasks();
  }

  test('LoadFakeChromeOSPage', async () => {
    await flushTasks();

    const subSections = getSubsections('chromeos-page-id');
    const expectedLayouts = getManager().getSubcategories(/**ChromeOS*/ 0);
    // Two subsections for ChromeOS (Window Management + Virtual Desks).
    assertEquals(expectedLayouts!.size, subSections!.length);

    const keyIterator = expectedLayouts!.keys();
    // Assert subsection title (Window management) matches expected value from
    // fake lookup.
    const windowManagementValue = keyIterator.next().value;
    assertEquals(
        (fakeSubCategories.get(windowManagementValue) as string),
        subSections[0]!.title);
    // Asert 2 accelerators are loaded for Window Management.
    assertEquals(
        (expectedLayouts!.get(windowManagementValue) as LayoutInfoList).length,
        subSections[0]!.acceleratorContainer!.length);

    // Assert subsection title (Virtual Desks) matches expected value from
    // fake lookup.
    const virtualDesksValue = keyIterator.next().value;
    assertEquals(
        fakeSubCategories.get(virtualDesksValue), subSections[1]!.title);
    // Asert 2 accelerators are loaded for Virtual Desks.
    assertEquals(
        (expectedLayouts!.get(virtualDesksValue) as LayoutInfoList).length,
        subSections[1]!.acceleratorContainer!.length);
  });

  test('LoadFakeBrowserPage', async () => {
    await flushTasks();

    const navPanel =
        getPage().shadowRoot!.querySelector('navigation-view-panel');
    const navSelector =
        navPanel!.shadowRoot!.querySelector('#navigationSelector')!
            .querySelector('navigation-selector');
    const navMenuItems =
        navSelector!.shadowRoot!.querySelector('#navigationSelectorMenu')!
            .querySelectorAll('.navigation-item') as NodeListOf<HTMLDivElement>;
    // Index[1] represents the Browser subgetPage().
    navMenuItems[1]!.click();

    await flushTasks();

    const subSections = getSubsections('browser-page-id');
    const expectedLayouts = getManager().getSubcategories(/**Browser*/ 1);
    // One subsection for the Browser (Tabs).
    assertEquals(expectedLayouts!.size, subSections!.length);

    const keyIterator = expectedLayouts!.keys().next();
    // Assert subsection names match name lookup (Tabs).
    assertEquals(
        fakeSubCategories.get(keyIterator.value), subSections[0]!.title);
    // Assert only 1 accelerator is within Tabs.
    assertEquals(
        (expectedLayouts!.get(keyIterator.value) as LayoutInfoList).length,
        subSections[0]!.acceleratorContainer.length);
  });

  test('OpenDialogFromAccelerator', async () => {
    await flushTasks();

    // The edit dialog should not be stamped and visible.
    let editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections('chromeos-page-id');
    const accelerators =
        subSections[0]!.shadowRoot!.querySelectorAll('accelerator-row');
    // Only two accelerators rows for "Window Management".
    assertEquals(2, accelerators.length);
    // Click on the first accelerator, expect the edit dialog to open.
    accelerators[0]!.click();
    await flushTasks();
    editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);
  });

  test('DialogOpensOnEvent', async () => {
    await flushTasks();

    // The edit dialog should not be stamped and visible.
    let editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const nav = getPage().shadowRoot!.querySelector('navigation-view-panel');

    const acceleratorInfo = createUserAccelerator(
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
    await flushTasks();

    // Open dialog for first accelerator in View Desk subsection.
    await openDialogForAcceleratorInSubsection(/*View Desk*/ 1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from Virtual Desks subsection.
    let editView =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view')[0] as AcceleratorEditViewElement;

    // Click on edit button.
    (editView!.shadowRoot!.querySelector('#editButton') as CrButtonElement)
        .click();

    await flushTasks();

    let accelViewElement =
        editView.shadowRoot!.querySelector('#acceleratorItem');

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editView.hasError);

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

    // Requery the view element.
    editView =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view')[0] as AcceleratorEditViewElement;
    accelViewElement = editView.shadowRoot!.querySelector('#acceleratorItem');

    // Assert that the accelerator was updated with the new shortcut (Alt + ']')
    const acceleratorInfo =
        (accelViewElement as AcceleratorViewElement).acceleratorInfo as
        AcceleratorInfo;
    const actualAccelerator = acceleratorInfo.accelerator as AcceleratorKeys;
    assertEquals(Modifier.ALT, actualAccelerator!.modifiers);
    assertEquals(221, actualAccelerator!.key);
    assertEquals(']', actualAccelerator!.keyDisplay);
  });

  test('AddAccelerator', async () => {
    await flushTasks();

    // Open dialog for first accelerator in View Desk subsection.
    await openDialogForAcceleratorInSubsection(/*View Desk*/ 1);
    const editDialog = getPage().shadowRoot!.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from Virtual Desks subsection.
    let dialogAccels =
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

    // Alt + ']' is a conflict, expect the error message to appear.
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

    // Press the shortcut again, this time it will add and remove the preexsting
    // accelerator.
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

    // Requery all accelerators.
    dialogAccels =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    // Expect 2 accelerators now.
    assertEquals(2, dialogAccels!.length);
    const newAccel = dialogAccels[1];

    const actualAccelerator =
        (newAccel!.shadowRoot!.querySelector('#acceleratorItem') as
         AcceleratorViewElement)
            .acceleratorInfo!.accelerator;
    assertEquals(Modifier.ALT, actualAccelerator!.modifiers);
    assertEquals(221, actualAccelerator!.key);
    assertEquals(']', actualAccelerator!.keyDisplay);
  });

  test('RemoveAccelerator', async () => {
    await flushTasks();

    // Open dialog for first accelerator in View Desk subsection.
    await openDialogForAcceleratorInSubsection(/*View Desk*/ 1);
    const editDialog = getDialog('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from Virtual Desks subsection.
    let acceleratorList =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');
    assertEquals(1, acceleratorList!.length);
    const editView = acceleratorList[0] as AcceleratorEditViewElement;

    // Click on remove button.
    const deleteButton =
        editView!.shadowRoot!.querySelector('#deleteButton') as CrButtonElement;
    deleteButton.click();

    await flushTasks();

    // Requery the accelerator elements.
    acceleratorList =
        editDialog!.shadowRoot!.querySelector('cr-dialog')!.querySelectorAll(
            'accelerator-edit-view');

    // Expect that the accelerator has now been removed.
    assertEquals(0, acceleratorList!.length);
  });

  test('RestoreAllButton', async () => {
    await flushTasks();

    let restoreDialog = getDialog('#restoreDialog');
    // Expect the dialog to not appear initially.
    assertFalse(!!restoreDialog);

    // Click on the Restore all button.
    const restoreButton = getPage().shadowRoot!.querySelector(
                              '#restoreAllButton') as CrButtonElement;
    restoreButton!.click();

    await flushTasks();

    // Requery the dialog.
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
});
