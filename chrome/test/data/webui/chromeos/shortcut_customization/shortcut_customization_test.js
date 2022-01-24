// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/accelerator_lookup_manager.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/accelerator_subsection.js';
import {fakeAcceleratorConfig, fakeLayoutInfo, fakeSubCategories} from 'chrome://shortcut-customization/fake_data.js';
import {getShortcutProvider, setShortcutProviderForTesting} from 'chrome://shortcut-customization/mojo_interface_provider.js';
import {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/shortcut_customization_app.js';
import {AcceleratorInfo, Modifier, ShortcutProviderInterface} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {CreateUserAccelerator} from './shortcut_customization_test_util.js';

export function shortcutCustomizationAppTest() {
  /** @type {?ShortcutCustomizationAppElement} */
  let page = null;

  /** @type {?AcceleratorLookupManager} */
  let manager = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();

    page = /** @type {!ShortcutCustomizationAppElement} */ (
        document.createElement('shortcut-customization-app'));
    document.body.appendChild(page);
  });

  teardown(() => {
    manager.reset();
    page.remove();
    page = null;
  });

  /**
   * @param {string} subpageId
   * @return {!Array<!AcceleratorSubsectionElement>}
   */
  function getSubsections_(subpageId) {
    const navPanel = page.shadowRoot.querySelector('navigation-view-panel');
    const navBody = navPanel.shadowRoot.querySelector('#navigationBody');
    const subPage = navBody.querySelector(`#${subpageId}`);
    return subPage.shadowRoot.querySelectorAll('accelerator-subsection');
  }

  /**
   * @param {number} subsectionIndex
   */
  async function openDialogForAcceleratorInSubsection_(subsectionIndex) {
    // The edit dialog should not be stamped and visible.
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections_('chromeos-page-id');
    const accelerators =
        subSections[subsectionIndex].shadowRoot.querySelectorAll(
            'accelerator-row');

    // Click on the first accelerator, expect the edit dialog to open.
    accelerators[0].click();
    await flushTasks();
  }

  test('LoadFakeChromeOSPage', async () => {
    await flushTasks();

    const subSections = getSubsections_('chromeos-page-id');
    const expectedLayouts = manager.getSubcategories(/**ChromeOS*/ 0);
    // Two subsections for ChromeOS (Window Management + Virtual Desks).
    assertEquals(expectedLayouts.size, subSections.length);

    let keyIterator = expectedLayouts.keys();
    // Assert subsection title (Window management) matches expected value from
    // fake lookup.
    const windowManagementValue = keyIterator.next().value;
    assertEquals(
        fakeSubCategories.get(windowManagementValue), subSections[0].title);
    // Asert 2 accelerators are loaded for Window Management.
    assertEquals(
        expectedLayouts.get(windowManagementValue).length,
        subSections[0].acceleratorContainer.length);

    // Assert subsection title (Virtual Desks) matches expected value from
    // fake lookup.
    const virtualDesksValue = keyIterator.next().value;
    assertEquals(
        fakeSubCategories.get(virtualDesksValue), subSections[1].title);
    // Asert 2 accelerators are loaded for Virtual Desks.
    assertEquals(
        expectedLayouts.get(virtualDesksValue).length,
        subSections[1].acceleratorContainer.length);
  });

  test('LoadFakeBrowserPage', async () => {
    await flushTasks();

    const navPanel = page.shadowRoot.querySelector('navigation-view-panel');
    const navSelector = navPanel.shadowRoot.querySelector('#navigationSelector')
                            .querySelector('navigation-selector');
    const navMenuItems =
        navSelector.shadowRoot.querySelector('#navigationSelectorMenu')
            .querySelectorAll('.navigation-item');
    // Index[1] represents the Browser subpage.
    navMenuItems[1].click();

    await flushTasks();

    const subSections = getSubsections_('browser-page-id');
    const expectedLayouts = manager.getSubcategories(/**Browser*/ 1);
    // One subsection for the Browser (Tabs).
    assertEquals(expectedLayouts.size, subSections.length);

    const keyIterator = expectedLayouts.keys().next();
    // Assert subsection names match name lookup (Tabs).
    assertEquals(
        fakeSubCategories.get(keyIterator.value), subSections[0].title);
    // Assert only 1 accelerator is within Tabs.
    assertEquals(
        expectedLayouts.get(keyIterator.value).length,
        subSections[0].acceleratorContainer.length);
  });

  test('OpenDialogFromAccelerator', async () => {
    await flushTasks();

    // The edit dialog should not be stamped and visible.
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const subSections = getSubsections_('chromeos-page-id');
    const accelerators =
        subSections[0].shadowRoot.querySelectorAll('accelerator-row');
    // Only two accelerators rows for "Window Management".
    assertEquals(2, accelerators.length);
    // Click on the first accelerator, expect the edit dialog to open.
    accelerators[0].click();
    await flushTasks();
    editDialog = page.shadowRoot.querySelector('#editDialog');
    assertTrue(!!editDialog);
  });

  test('DialogOpensOnEvent', async () => {
    await flushTasks();

    // The edit dialog should not be stamped and visible.
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const nav = page.shadowRoot.querySelector('navigation-view-panel');

    /** @type {!AcceleratorInfo} */
    const acceleratorInfo = CreateUserAccelerator(
        Modifier.SHIFT,
        /*key=*/ 67,
        /*key_display=*/ 'c');

    // Simulate the trigger event to display the dialog.
    nav.dispatchEvent(new CustomEvent('show-edit-dialog', {
      bubbles: true,
      composed: true,
      detail: /**@type {!Object}*/ (
          {description: 'test', accelerators: [acceleratorInfo]})
    }));
    await flushTasks();

    // Requery dialog.
    editDialog = page.shadowRoot.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Close the dialog.
    const dialog = editDialog.shadowRoot.querySelector('#editDialog');
    dialog.close();
    await flushTasks();

    assertFalse(dialog.open);
  });

  test('ReplaceAccelerator', async () => {
    await flushTasks();

    // Open dialog for first accelerator in View Desk subsection.
    await openDialogForAcceleratorInSubsection_(/*View Desk*/ 1);
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from Virtual Desks subsection.
    let editView = editDialog.shadowRoot.querySelector('cr-dialog')
                       .querySelectorAll('accelerator-edit-view')[0];

    // Click on edit button.
    editView.shadowRoot.querySelector('#editButton').click();

    await flushTasks();

    let accelViewElement =
        editView.shadowRoot.querySelector('#acceleratorItem');

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editView.hasError);

    // Alt + ']' is a conflict, expect the error message to appear.
    accelViewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: '221',
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
    accelViewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: '221',
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();

    // Requery the view element.
    editView = editDialog.shadowRoot.querySelector('cr-dialog')
                   .querySelectorAll('accelerator-edit-view')[0];
    accelViewElement = editView.shadowRoot.querySelector('#acceleratorItem');

    // Assert that the accelerator was updated with the new shortcut (Alt + ']')
    const actualAccelerator = accelViewElement.acceleratorInfo.accelerator;
    assertEquals(Modifier.ALT, actualAccelerator.modifiers);
    assertEquals(221, actualAccelerator.key);
    assertEquals(']', actualAccelerator.key_display);
  });

  test('AddAccelerator', async () => {
    await flushTasks();

    // Open dialog for first accelerator in View Desk subsection.
    await openDialogForAcceleratorInSubsection_(/*View Desk*/ 1);
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from Virtual Desks subsection.
    let dialogAccels = editDialog.shadowRoot.querySelector('cr-dialog')
                           .querySelectorAll('accelerator-edit-view');
    // Expect only 1 accelerator initially.
    assertEquals(1, dialogAccels.length);

    // Click on add button.
    editDialog.shadowRoot.querySelector('#addAcceleratorButton').click();

    await flushTasks();

    const editElement =
        editDialog.shadowRoot.querySelector('#pendingAccelerator');

    // Assert no error has occurred prior to pressing a shortcut.
    assertFalse(editElement.hasError);

    const viewElement =
        editElement.shadowRoot.querySelector('#acceleratorItem');

    // Alt + ']' is a conflict, expect the error message to appear.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: '221',
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
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: ']',
      keyCode: '221',
      code: 'Key]',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();

    // Requery all accelerators.
    dialogAccels = editDialog.shadowRoot.querySelector('cr-dialog')
                       .querySelectorAll('accelerator-edit-view');
    // Expect 2 accelerators now.
    assertEquals(2, dialogAccels.length);
    const newAccel = dialogAccels[1];

    const actualAccelerator =
        newAccel.shadowRoot.querySelector('#acceleratorItem')
            .acceleratorInfo.accelerator;
    assertEquals(Modifier.ALT, actualAccelerator.modifiers);
    assertEquals(221, actualAccelerator.key);
    assertEquals(']', actualAccelerator.key_display);
  });

  test('RemoveAccelerator', async () => {
    await flushTasks();

    // Open dialog for first accelerator in View Desk subsection.
    await openDialogForAcceleratorInSubsection_(/*View Desk*/ 1);
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Grab the first accelerator from Virtual Desks subsection.
    let acceleratorList = editDialog.shadowRoot.querySelector('cr-dialog')
                              .querySelectorAll('accelerator-edit-view');
    assertEquals(1, acceleratorList.length);
    const editView = acceleratorList[0];

    // Click on remove button.
    editView.shadowRoot.querySelector('#deleteButton').click();

    await flushTasks();

    // Requery the accelerator elements.
    acceleratorList = editDialog.shadowRoot.querySelector('cr-dialog')
                          .querySelectorAll('accelerator-edit-view');

    // Expect that the accelerator has now been removed.
    assertEquals(0, acceleratorList.length);
  });

  test('RestoreAllButton', async () => {
    await flushTasks();

    let restoreDialog = page.shadowRoot.querySelector('#restoreDialog');
    // Expect the dialog to not appear initially.
    assertFalse(!!restoreDialog);

    // Click on the Restore all button.
    const restoreButton = page.shadowRoot.querySelector('#restoreAllButton');
    restoreButton.click();

    await flushTasks();

    // Requery the dialog.
    restoreDialog = page.shadowRoot.querySelector('#restoreDialog');
    assertTrue(restoreDialog.open);

    // Click on Cancel button.
    restoreDialog.querySelector('#cancelButton').click();

    await flushTasks();

    restoreDialog = page.shadowRoot.querySelector('#restoreDialog');
    assertFalse(!!restoreDialog);
  });

  suite('FakeMojoProviderTest', () => {
    test('SettingGettingTestProvider', () => {
      // TODO(zentaro): Replace with fake when built.
      let fake_provider =
          /** @type {!ShortcutProviderInterface} */ (new Object());
      setShortcutProviderForTesting(fake_provider);
      assertEquals(fake_provider, getShortcutProvider());
    });
  });
}
