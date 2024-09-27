// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://batch-upload/batch_upload.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert} from '//resources/js/assert.js';
import {BatchUploadBrowserProxyImpl} from 'chrome://batch-upload/batch_upload.js';
import type {BatchUploadAppElement, BatchUploadData, DataContainer, DataItem, DataSectionElement, PageRemote} from 'chrome://batch-upload/batch_upload.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBatchUploadBrowserProxy} from './test_batch_upload_browser_proxy.js';

suite('BatchUploadViewTest', function() {
  let batchUploadApp: BatchUploadAppElement;
  let testBatchUploadProxy: TestBatchUploadBrowserProxy;
  let callbackRouterRemote: PageRemote;

  // Input of data displayed in the Ui containing account info, data sections
  // and dialog subtitles. In each section, some data items are pushed; check
  // `prepareDataInput()`.
  const TEST_DATA: BatchUploadData = prepareDataInput();

  // Prepare data for testing, simulating information coming from the browser.
  function prepareDataInput(): BatchUploadData {
    // Create passwords section.
    const password1: DataItem = {
      id: 1,
      iconUrl: 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE',
      title: 'password1',
      subtitle: 'username1',
    };
    const password2: DataItem = {
      id: 2,
      iconUrl: 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE',
      title: 'password2',
      subtitle: 'username2',
    };
    const passwordSection: DataContainer = {
      sectionTitle: 'Passwords',
      dataItems: [],
    };
    passwordSection.dataItems.push(password1);
    passwordSection.dataItems.push(password2);

    // Create addresses section
    const address: DataItem = {
      id: 1,
      iconUrl: '',
      title: 'address',
      subtitle: 'street',
    };
    const addressSection: DataContainer = {
      dataItems: [],
      sectionTitle: 'Addresses',
    };
    addressSection.dataItems.push(address);

    const dataContainers: DataContainer[] = [];
    dataContainers.push(passwordSection);
    dataContainers.push(addressSection);

    const dataInput: BatchUploadData = {
      accountInfo: {email: 'test@test.com', dataPictureUrl: ''},
      dialogSubtitle: '2 passwords and other items ...',
      dataContainers: dataContainers,
    };

    return dataInput;
  }

  function getTotalNumberOfItemsInSection(index: number): number {
    assert(TEST_DATA);
    assert(TEST_DATA.dataContainers[index]);
    return TEST_DATA.dataContainers[index].dataItems.length;
  }

  function getSectionElement(index: number): DataSectionElement {
    const dataSections =
        batchUploadApp.shadowRoot!.querySelectorAll('data-section');
    assertEquals(TEST_DATA.dataContainers.length, dataSections.length);
    assertLT(index, TEST_DATA.dataContainers.length);
    return dataSections[index]!;
  }

  setup(function() {
    testBatchUploadProxy = new TestBatchUploadBrowserProxy();
    callbackRouterRemote =
        testBatchUploadProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BatchUploadBrowserProxyImpl.setInstance(testBatchUploadProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    batchUploadApp = document.createElement('batch-upload-app');
    document.body.append(batchUploadApp);
    callbackRouterRemote.sendBatchUploadData(TEST_DATA);
    return testBatchUploadProxy.handler.whenCalled('updateViewHeight');
  });

  test('HeaderContent', function() {
    assertTrue(isVisible(batchUploadApp));

    // Header.
    assertTrue(isChildVisible(batchUploadApp, '#header'));
    assertTrue(isChildVisible(batchUploadApp, '#title'));
    assertTrue(isChildVisible(batchUploadApp, '#subtitle'));
    assertGT(TEST_DATA.dataContainers.length, 1);
    assertEquals(
        TEST_DATA.dialogSubtitle,
        batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                      '#subtitle')!.textContent!.trim());

    // Account info.
    assertTrue(isChildVisible(batchUploadApp, '#account-info-row'));
    assertTrue(isChildVisible(batchUploadApp, '#account-icon'));
    assertTrue(isChildVisible(batchUploadApp, '#email'));
    assertEquals(
        TEST_DATA.accountInfo.email,
        batchUploadApp.shadowRoot!
            .querySelector<CrButtonElement>(
                '#account-info-row')!.textContent!.trim());
  });

  test('ClickSave', async function() {
    assertTrue(isVisible(batchUploadApp));

    batchUploadApp.$.saveButton.click();

    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(TEST_DATA.dataContainers.length, idsToMove.length);
    // By default all items are selected and should be part of the result.
    const expectedSelectedItemsIds = [[1, 2], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });

  test('ClickCancel', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#cancelButton'));
    batchUploadApp.$.cancelButton.click();
    return testBatchUploadProxy.handler.whenCalled('close');
  });

  test('DataSections', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#dataSections'));

    const dataSections =
        batchUploadApp.shadowRoot!.querySelectorAll('data-section');
    assertEquals(TEST_DATA.dataContainers.length, dataSections.length);
  });

  test('ClickSaveWithUnselectingItems', async function() {
    assertTrue(isVisible(batchUploadApp));

    const firstIndex = 0;
    const firstSection = getSectionElement(firstIndex);
    assertTrue(isVisible(firstSection));

    // Get all checkboxes for each sections ordered.
    const firstSectionCheckboxes =
        firstSection.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(
        getTotalNumberOfItemsInSection(firstIndex),
        firstSectionCheckboxes.length);

    // Unselect the second item.
    const secondCheckbox = firstSectionCheckboxes[1]!;
    assertTrue(secondCheckbox.checked);
    secondCheckbox.click();
    await microtasksFinished();

    // Click Save.
    batchUploadApp.$.saveButton.click();

    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(TEST_DATA.dataContainers.length, idsToMove.length);
    // Expected result does not contain the removed Id.
    const expectedSelectedItemsIds = [[1], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });

  test('ClickSaveWithUnselectingAFullSection', async function() {
    assertTrue(isVisible(batchUploadApp));

    const firstIndex = 0;
    const firstSection = getSectionElement(firstIndex);
    assertTrue(isVisible(firstSection));

    // Get the section checkboxes ordered.
    const firstSectionCheckboxes =
        firstSection.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(
        getTotalNumberOfItemsInSection(firstIndex),
        firstSectionCheckboxes.length);

    const firstToggle = firstSection.$.toggle;
    // First toggle is enabled by default.
    assertTrue(firstToggle.checked);

    // Unselect the first and second item, making the first section completely
    // unselected.
    const firstCheckbox = firstSectionCheckboxes[0]!;
    const secondCheckbox = firstSectionCheckboxes[1]!;
    assertTrue(firstCheckbox.checked);
    assertTrue(secondCheckbox.checked);
    firstCheckbox.click();
    secondCheckbox.click();
    await microtasksFinished();

    // First section should now be marked as disabled.
    assertFalse(firstToggle.checked);

    // Click Save.
    batchUploadApp.$.saveButton.click();

    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(TEST_DATA.dataContainers.length, idsToMove.length);
    // A section with no element selected should still appear as empty to
    // keep the consistency of the input/output.
    const expectedSelectedItemsIds = [[], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });

  test('ClickSaveWithToggledOffSections', async function() {
    assertTrue(isVisible(batchUploadApp));

    const firstIndex = 0;
    const firstSection = getSectionElement(firstIndex);
    assertTrue(isVisible(firstSection));

    const firstToggle = firstSection.$.toggle;
    // First toggle is enabled by default.
    assertTrue(firstToggle.checked);

    // Disable first section.
    assertTrue(firstToggle.checked);
    firstToggle.click();
    await microtasksFinished();

    // Click Save.
    batchUploadApp.$.saveButton.click();
    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(idsToMove.length, TEST_DATA.dataContainers.length);
    // A section with the toggle off should still appear as empty even if
    // it had some selected items before the toggle off.
    const expectedSelectedItemsIds = [[], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });

  test('TogglingOffAllSectionsShouldDisableSave', async function() {
    assertTrue(isVisible(batchUploadApp));

    const saveButton = batchUploadApp.$.saveButton;
    // By default save is enabled.
    assertFalse(saveButton.disabled);

    const dataSections =
        batchUploadApp.shadowRoot!.querySelectorAll('data-section');
    assertEquals(dataSections.length, TEST_DATA.dataContainers.length);

    // Disable all toggles.
    for (let i = 0; i < dataSections.length; ++i) {
      const toggle = dataSections[i]!.$.toggle;
      assertTrue(toggle.checked);
      toggle.click();
    }
    await microtasksFinished();

    // After updates the button should now be disabled.
    assertTrue(saveButton.disabled);
  });

  test('UncheckingAllCheckboxesShouldDisableSave', async function() {
    assertTrue(isVisible(batchUploadApp));

    const saveButton = batchUploadApp.$.saveButton;
    // By default save is enabled.
    assertFalse(saveButton.disabled);

    const dataSections =
        batchUploadApp.shadowRoot!.querySelectorAll('data-section');
    assertEquals(dataSections.length, TEST_DATA.dataContainers.length);

    // Unckeck all checkboxes of each section to toggle the sections off.
    for (let i = 0; i < dataSections.length; ++i) {
      const section = getSectionElement(i);

      const toggle = section.$.toggle;
      assertTrue(toggle.checked);

      const checkboxes =
          section.shadowRoot!.querySelectorAll<CrCheckboxElement>(
              '.item-checkbox');
      for (let j = 0; j < checkboxes.length; ++j) {
        const checkbox = checkboxes[j]!;
        assertTrue(checkbox.checked);
        checkbox.click();
      }
      await microtasksFinished();
      assertFalse(toggle.checked);
    }

    // With all section toggled off by unchecking the inner checkboxes, the
    // button should be disabled.
    assertTrue(saveButton.disabled);
  });
});
