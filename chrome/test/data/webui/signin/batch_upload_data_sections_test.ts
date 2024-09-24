// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://batch-upload/batch_upload.js';

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {DataContainer, DataItem, DataSectionElement} from 'chrome://batch-upload/batch_upload.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('BatchUploadViewTest', function() {
  let dataSectionElement: DataSectionElement;

  // Section Input.
  const TEST_DATA: DataContainer = prepareDataInput();

  function prepareDataInput(): DataContainer {
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

    return passwordSection;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dataSectionElement = document.createElement('data-section');
    dataSectionElement.dataContainer = TEST_DATA;
    document.body.append(dataSectionElement);
  });

  test('MainSectionComponents', function() {
    assertTrue(isVisible(dataSectionElement));

    assertTrue(isChildVisible(dataSectionElement, '.data-section-title'));
    assertTrue(isChildVisible(dataSectionElement, '.expand-button'));
    assertTrue(isChildVisible(dataSectionElement, '.separator'));
    assertTrue(isChildVisible(dataSectionElement, '.toggle'));
    assertFalse(isChildVisible(dataSectionElement, '.data-items-collapse'));
  });

  test('SectionTitles', async function() {
    assertTrue(isVisible(dataSectionElement));

    const sectionTitle =
        dataSectionElement.shadowRoot!.querySelector('.data-section-title')!;
    const numberOfItems = TEST_DATA.dataItems.length;
    await microtasksFinished();
    // All items are selected by default and should be shown in the title.
    assertTrue(
        sectionTitle.textContent!.trim().includes('(' + numberOfItems + ')'));

    // Uncheck the first item.
    const checkboxes =
        dataSectionElement.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(numberOfItems, checkboxes.length);
    const firstCheckbox = checkboxes[0]!;
    assertTrue(firstCheckbox.checked);
    firstCheckbox.click();
    // Waiting for the Ui update that would affect the section title.
    await microtasksFinished();

    // Selected items items count should show, so 1 less.
    assertTrue(sectionTitle.textContent!.trim().includes(
        '(' + (numberOfItems - 1) + ')'));
  });

  test('ExpandingSections', async function() {
    assertTrue(isVisible(dataSectionElement));

    const expandButton =
        dataSectionElement.shadowRoot!.querySelector<CrExpandButtonElement>(
            '.expand-button')!;
    const collapsePart =
        dataSectionElement.shadowRoot!.querySelector<CrCollapseElement>(
            '.data-items-collapse')!;
    assertTrue(isVisible(expandButton));

    // Section is collapsed by default.
    assertFalse(collapsePart.opened);

    // Expanding the section on click.
    expandButton.click();
    await microtasksFinished();
    assertTrue(collapsePart.opened);

    // Collapsing the section second click.
    expandButton.click();
    await microtasksFinished();
    assertFalse(collapsePart.opened);
  });

  test('AllItemsSelectedByDefault', function() {
    assertTrue(isVisible(dataSectionElement));

    // Check that all items are selected by default.
    const itemCheckboxes =
        dataSectionElement!.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(TEST_DATA.dataItems.length, itemCheckboxes.length);
    for (let j = 0; j < itemCheckboxes.length; ++j) {
      assertTrue(itemCheckboxes[j]!.checked);
    }

    assertDeepEquals(new Set<number>([1, 2]), dataSectionElement.dataSelected);
  });

  test('SeletctedItemsWithOutput', async function() {
    assertTrue(isVisible(dataSectionElement));

    // Check that all items are selected by default.
    const itemCheckboxes =
        dataSectionElement!.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(TEST_DATA.dataItems.length, itemCheckboxes.length);

    itemCheckboxes[0]!.click();
    await microtasksFinished();
    assertDeepEquals(new Set<number>([2]), dataSectionElement.dataSelected);

    itemCheckboxes[0]!.click();
    await microtasksFinished();
    assertDeepEquals(new Set<number>([1, 2]), dataSectionElement.dataSelected);

    itemCheckboxes[0]!.click();
    itemCheckboxes[1]!.click();
    await microtasksFinished();
    assertDeepEquals(new Set<number>(), dataSectionElement.dataSelected);
  });
});
