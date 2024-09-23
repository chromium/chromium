// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://batch-upload/batch_upload.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {assert} from '//resources/js/assert.js';
import {BatchUploadBrowserProxyImpl} from 'chrome://batch-upload/batch_upload.js';
import type {BatchUploadAppElement, DataContainer, DataItem, PageRemote} from 'chrome://batch-upload/batch_upload.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBatchUploadBrowserProxy} from './test_batch_upload_browser_proxy.js';

suite('BatchUploadViewTest', function() {
  let batchUploadApp: BatchUploadAppElement;
  let testBatchUploadProxy: TestBatchUploadBrowserProxy;
  let callbackRouterRemote: PageRemote;

  // Input mapping to the number of sections displayed. In each section, some
  // data items are pushed; check `prepareDataInput`.
  const TEST_DATA: DataContainer[] = prepareDataInput();
  // Total number of items in `TEST_DATA`.
  const TOTAL_ITEM_COUNT = getTotalNumberOfInputItems();

  // Prepare data for testing, simulating information coming from the browser.
  function prepareDataInput(): DataContainer[] {
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
      dialogSubtitle: '2 passwords',
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
      dialogSubtitle: '',
    };
    addressSection.dataItems.push(address);

    const dataInput: DataContainer[] = [];
    dataInput.push(passwordSection);
    dataInput.push(addressSection);
    return dataInput;
  }

  function getTotalNumberOfInputItems(): number {
    assert(TEST_DATA);
    // Count the total number of items by getting each section length and
    // accumulating them for the total count.
    return TEST_DATA
        .map((section: DataContainer) => {
          return section.dataItems.length;
        })
        .reduce((sum, sectionLength) => {
          return sum + sectionLength;
        });
  }

  setup(function() {
    testBatchUploadProxy = new TestBatchUploadBrowserProxy();
    callbackRouterRemote =
        testBatchUploadProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BatchUploadBrowserProxyImpl.setInstance(testBatchUploadProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    batchUploadApp = document.createElement('batch-upload-app');
    document.body.append(batchUploadApp);
    callbackRouterRemote.sendDataItems(TEST_DATA);
    return testBatchUploadProxy.handler.whenCalled('updateViewHeight');
  });

  test('HeaderContent', function() {
    assertTrue(isVisible(batchUploadApp));

    // Header.
    assertTrue(isChildVisible(batchUploadApp, '#header'));
    assertTrue(isChildVisible(batchUploadApp, '#title'));
    assertTrue(isChildVisible(batchUploadApp, '#subtitle'));
    assertGT(TEST_DATA.length, 1);
    // TODO(b/365954465): Adapt based on latest decision of the subtitle.
    assertEquals(
        TEST_DATA[0]!.dialogSubtitle,
        batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                      '#subtitle')!.textContent!.trim());

    // Account info.
    assertTrue(isChildVisible(batchUploadApp, '#account-info-row'));
    assertTrue(isChildVisible(batchUploadApp, '#account-icon'));
    assertTrue(isChildVisible(batchUploadApp, '#email'));
    // TODO(b/359796907): adapt when properly sending account information.
    assertEquals(
        'elisa.g.beckett@gmail.com',
        batchUploadApp.shadowRoot!
            .querySelector<CrButtonElement>(
                '#account-info-row')!.textContent!.trim());
  });

  test('ClickSave', async function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#save-button'));
    batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                  '#save-button')!.click();

    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(TEST_DATA.length, idsToMove.length);
    // By default all items are selected and should be part of the result.
    const expectedSelectedItemsIds = [[1, 2], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });

  test('ClickClose', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#close-button'));
    batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                  '#close-button')!.click();
    return testBatchUploadProxy.handler.whenCalled('close');
  });

  test('DataSections', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#data-sections'));

    const dataSections =
        batchUploadApp.shadowRoot!.querySelectorAll('.data-section');
    assertEquals(TEST_DATA.length, dataSections.length);
  });

  test('SectionTitles', async function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '.data-section-header'));

    const sectionTitles =
        batchUploadApp.shadowRoot!.querySelectorAll('.data-section-title');
    assertEquals(TEST_DATA.length, sectionTitles.length);

    const firstSectionTitle = sectionTitles[0]!;
    const numberOfItemsInFirstSection = TEST_DATA[0]!.dataItems.length;
    // All items are selected by default and should be shown in the title.
    assertTrue(firstSectionTitle.textContent!.trim().includes(
        '(' + numberOfItemsInFirstSection + ')'));

    // Uncheck the first item of the first section.
    const itemCheckboxes =
        batchUploadApp.shadowRoot!.querySelectorAll('.item-checkbox');
    assertEquals(TOTAL_ITEM_COUNT, itemCheckboxes.length);
    const firstCheckbox = itemCheckboxes[0] as CrCheckboxElement;
    assertTrue(firstCheckbox.checked);
    firstCheckbox.click();
    // Waiting for the Ui update that would affect the section title.
    await microtasksFinished();

    // Selected items items count should show, so 1 less.
    assertTrue(firstSectionTitle.textContent!.trim().includes(
        '(' + (numberOfItemsInFirstSection - 1) + ')'));
  });

  test('ExpandingSections', async function() {
    assertTrue(isVisible(batchUploadApp));

    const expandButtons =
        batchUploadApp.shadowRoot!.querySelectorAll('.expand-button');
    const collapseSections =
        batchUploadApp.shadowRoot!.querySelectorAll('.data-items-collapse');
    assertEquals(TEST_DATA.length, expandButtons.length);
    assertEquals(TEST_DATA.length, collapseSections.length);

    const firstCollapseSection = collapseSections[0] as CrCollapseElement;
    const firstExpandButton = expandButtons[0]! as CrExpandButtonElement;
    // Section is collapsed by default.
    assertFalse(firstCollapseSection.opened);

    // Expanding the section on click.
    firstExpandButton.click();
    await microtasksFinished();
    assertTrue(firstCollapseSection.opened);

    // Collapsing the section second click.
    firstExpandButton.click();
    await microtasksFinished();
    assertFalse(firstCollapseSection.opened);
  });

  test('AllItemsSelectedByDefault', async function() {
    assertTrue(isVisible(batchUploadApp));

    const itemCheckboxes =
        batchUploadApp.shadowRoot!.querySelectorAll('.item-checkbox');
    assertEquals(TOTAL_ITEM_COUNT, itemCheckboxes.length);

    // Check that all items are selected by default.
    for (let i = 0; i < itemCheckboxes.length; ++i) {
      assertTrue((itemCheckboxes[i] as CrCheckboxElement).checked);
    }
  });

  test('ClickSaveWithUnselectingItems', async function() {
    assertTrue(isVisible(batchUploadApp));

    // Get all checkboxes for each sections ordered.
    const itemCheckboxes =
        batchUploadApp.shadowRoot!.querySelectorAll('.item-checkbox');
    assertEquals(TOTAL_ITEM_COUNT, itemCheckboxes.length);

    // Unselect the second item.
    const secondCheckbox = itemCheckboxes[1] as CrCheckboxElement;
    assertTrue(secondCheckbox.checked);
    secondCheckbox.click();
    await microtasksFinished();

    // Click Save.
    assertTrue(isChildVisible(batchUploadApp, '#save-button'));
    batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                  '#save-button')!.click();

    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(TEST_DATA.length, idsToMove.length);
    // Expected result does not contain the removed Id.
    const expectedSelectedItemsIds = [[1], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });

  test('ClickSaveWithUnselectingAFullSection', async function() {
    assertTrue(isVisible(batchUploadApp));

    // Get all checkboxes for each sections ordered.
    const itemCheckboxes =
        batchUploadApp.shadowRoot!.querySelectorAll('.item-checkbox');
    assertEquals(TOTAL_ITEM_COUNT, itemCheckboxes.length);

    // Unselect the first and second item, making the first section completely
    // unselected.
    const firstCheckbox = itemCheckboxes[0] as CrCheckboxElement;
    const secondCheckbox = itemCheckboxes[1] as CrCheckboxElement;
    assertTrue(firstCheckbox.checked);
    assertTrue(secondCheckbox.checked);
    firstCheckbox.click();
    secondCheckbox.click();
    await microtasksFinished();

    // Click Save.
    assertTrue(isChildVisible(batchUploadApp, '#save-button'));
    batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                  '#save-button')!.click();

    const idsToMove =
        await testBatchUploadProxy.handler.whenCalled('saveToAccount');
    assertEquals(TEST_DATA.length, idsToMove.length);
    // A section with no element selected should still appear as empty to
    // keep the consistency of the input/output.
    const expectedSelectedItemsIds = [[], [1]];
    assertDeepEquals(expectedSelectedItemsIds, idsToMove);
  });
});
