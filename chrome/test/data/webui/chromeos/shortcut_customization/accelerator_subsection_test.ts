// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_subsection.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/js/accelerator_subsection.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {AcceleratorCategory, AcceleratorSource, AcceleratorSubcategory, LayoutInfo, LayoutStyle, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorSubsectionTest', function() {
  let sectionElement: AcceleratorSubsectionElement|null = null;

  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    manager = AcceleratorLookupManager.getInstance();
    manager!.setAcceleratorLookup(fakeAcceleratorConfig);
    manager!.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager!.reset();
    }
    sectionElement!.remove();
    sectionElement = null;
  });

  async function initAcceleratorSubsectionElement(
      category: AcceleratorCategory, subcategory: AcceleratorSubcategory) {
    sectionElement = document.createElement('accelerator-subsection');
    sectionElement.category = category;
    sectionElement.subcategory = subcategory;
    document.body.appendChild(sectionElement);
    return flushTasks();
  }

  // TODO(jimmyxgong): Update this test after retrieving accelerators is
  // implemented for a subsection.
  test('LoadsBasicSection', async () => {
    await initAcceleratorSubsectionElement(
        AcceleratorCategory.kWindowsAndDesks, AcceleratorSubcategory.kWindows);
    const acceleratorInfo1 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const expectedAccelInfos = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';
    const title = 'test title';
    const expectedLayoutInfo: LayoutInfo = {
      action: 0,
      category: AcceleratorCategory.kWindowsAndDesks,
      description,
      source: AcceleratorSource.kAsh,
      style: LayoutStyle.kDefault,
      subCategory: AcceleratorSubcategory.kWindows,
    };

    sectionElement!.title = title;
    sectionElement!.accelRowDataArray = [{
      acceleratorInfos: expectedAccelInfos,
      layoutInfo: expectedLayoutInfo,
    }];

    await flush();
    assertEquals(
        title,
        sectionElement!.shadowRoot!.querySelector(
                                       '#title')!.textContent!.trim());
  });

  test('LoadCategoryAndConfirmDescriptions', async () => {
    await initAcceleratorSubsectionElement(
        AcceleratorCategory.kWindowsAndDesks, AcceleratorSubcategory.kWindows);
    const expectedTitle = 'test title';
    sectionElement!.title = expectedTitle;

    await flushTasks();

    const rowListElement =
        sectionElement!.shadowRoot!.querySelectorAll('accelerator-row');

    // First accelerator-row corresponds to 'Snap Window Left', and its
    // subcategory is kWindows.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 0)!,
        rowListElement[0]!.description);
    assertEquals(
        manager!.getAcceleratorSubcategory(/*source=*/ 0, /*action=*/ 0)!,
        AcceleratorSubcategory.kWindows);
    // Second accelerator-row corresponds to 'Snap Window Right', and its
    // subcategory is kWindows.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 1)!,
        rowListElement[1]!.description);
    assertEquals(
        manager!.getAcceleratorSubcategory(/*source=*/ 0, /*action=*/ 1)!,
        AcceleratorSubcategory.kWindows);
  });

  test('ShowEmptyRowWhenCertainKeysAreUnavailable', async () => {
    await initAcceleratorSubsectionElement(
        AcceleratorCategory.kGeneral, AcceleratorSubcategory.kApps);
    const expectedTitle = 'test title';
    sectionElement!.title = expectedTitle;

    await flushTasks();

    const rowListElement =
        sectionElement!.shadowRoot!.querySelectorAll('accelerator-row');

    // There are two accelerators in General -> Apps category: 'Open
    // Calculator app' and 'Open Diagnostic app'.
    assertEquals(2, rowListElement.length);

    // First accelerator row in General -> Apps category
    // corresponds to 'Open Diagnostic app'. And its subcategory is kApps.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 5)!,
        rowListElement[1]!.description);
    assertEquals(
        manager!.getAcceleratorSubcategory(/*source=*/ 0, /*action=*/ 5)!,
        AcceleratorSubcategory.kApps);
    let shortcutsAssignedElement =
        rowListElement[1]!.shadowRoot!.querySelector(
            '#noShortcutAssignedContainer') as HTMLDivElement;
    assertTrue(shortcutsAssignedElement.hidden);

    // Second accelerator row in General -> Apps subcategory corresponds to
    // 'Open calculator app'. It should have an empty row.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 4)!,
        rowListElement[0]!.description);
    assertEquals(
        manager!.getAcceleratorSubcategory(/*source=*/ 0, /*action=*/ 4)!,
        AcceleratorSubcategory.kApps);
    // Expect the `noShortcutsAssigned` view to be available.
    shortcutsAssignedElement =
        rowListElement[0]!.shadowRoot!.querySelector(
            '#noShortcutAssignedContainer') as HTMLDivElement;
    assertFalse(shortcutsAssignedElement.hidden);

    // Expect 'noShortcutsAssigned' has an edit button.
    const editButton = strictQuery(
        '.edit-button', rowListElement[0]!.shadowRoot, CrIconButtonElement);
    assertTrue(!!editButton);

    // Add event listend and verify clicking edit-button will open the dialog.
    let showDialogListenerCalled = false;
    rowListElement[0]!.addEventListener('show-edit-dialog', () => {
      showDialogListenerCalled = true;
    });

    editButton.click();
    await flushTasks();

    // Expect the dialog is opened.
    assertTrue(showDialogListenerCalled);
  });

  test('RemoveAcceleratorWhenCertainKeysAreUnavailable', async () => {
    await initAcceleratorSubsectionElement(
        AcceleratorCategory.kGeneral, AcceleratorSubcategory.kGeneralControls);
    const expectedTitle = 'test title';
    sectionElement!.title = expectedTitle;

    await flushTasks();

    const rowListElement =
        sectionElement!.shadowRoot!.querySelectorAll('accelerator-row');

    // 'Open/close Google assistant' has two accelerators:
    // 1. [Search] + [A].
    // 2. [LauncheAssistant] key.
    // In fakeData, [LauncheAssistant] key is set to be unavailable and the
    // accelerator state is kDisabledByUnavailableKey. Therefore, only one
    // accelerator will be shown.
    assertEquals(1, rowListElement[0]!.acceleratorInfos.length);

    // First and the only accelerator row in General -> GeneralControls category
    // corresponds to 'Open/close Google assistant', and its subcategory is
    // kGeneralControls.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 6)!,
        rowListElement[0]!.description);
    assertEquals(
        manager!.getAcceleratorSubcategory(/*source=*/ 0, /*action=*/ 6)!,
        AcceleratorSubcategory.kGeneralControls);
  });

});
