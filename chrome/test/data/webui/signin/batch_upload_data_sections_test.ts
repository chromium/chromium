// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://batch-upload/batch_upload.js';

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
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

    assertTrue(isChildVisible(dataSectionElement, '#sectionTitle'));
    assertTrue(isChildVisible(dataSectionElement, '#expandButton'));
    assertTrue(isChildVisible(dataSectionElement, '#separator'));
    assertTrue(isChildVisible(dataSectionElement, '#toggle'));
    assertFalse(isChildVisible(dataSectionElement, '#collapse'));
  });

  test('SectionTitles', async function() {
    assertTrue(isVisible(dataSectionElement));

    const sectionTitle = dataSectionElement.$.sectionTitle;
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

    const expandButton = dataSectionElement.$.expandButton;
    const collapsePart = dataSectionElement.$.collapse;
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

  test('AllItemsSelectedByDefault', async function() {
    assertTrue(isVisible(dataSectionElement));
    await microtasksFinished();

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
    await microtasksFinished();

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

  test('DisablingSectionAffectsSectionHeader', async function() {
    assertTrue(isVisible(dataSectionElement));
    await microtasksFinished();

    const sectionTitle = dataSectionElement.$.sectionTitle;
    const numberOfItemsInSection = TEST_DATA.dataItems.length;
    const expectedInitialTitleExtraInfo = '(' + numberOfItemsInSection + ')';
    // Initial title name check.
    assertTrue(sectionTitle.textContent!.trim().includes(
        expectedInitialTitleExtraInfo));

    // Initial section divs state.
    const separator = dataSectionElement.$.separator;
    const expandButton = dataSectionElement.$.expandButton;
    const collapseSection = dataSectionElement.$.collapse;
    assertTrue(isVisible(separator));
    assertTrue(isVisible(expandButton));
    assertFalse(collapseSection.opened);
    // Open the first collapse section.
    expandButton.click();
    await microtasksFinished();
    assertTrue(collapseSection.opened);

    const toggle = dataSectionElement.$.toggle;
    assertTrue(toggle.checked);
    // Uncheck toggle.
    toggle.click();
    await microtasksFinished();

    // Info about selected count should not be present anymore.
    assertFalse(sectionTitle.textContent!.trim().includes(
        expectedInitialTitleExtraInfo));
    // Text should actually be only equal to the section title.
    assertEquals(TEST_DATA.sectionTitle, sectionTitle.textContent!.trim());
    assertFalse(isVisible(separator));
    assertFalse(isVisible(expandButton));
    assertFalse(collapseSection.opened);
  });

  test('ToggleOffThenOnSectionResetEffects', async function() {
    assertTrue(isVisible(dataSectionElement));

    // Expand the section
    const expandButton = dataSectionElement.$.expandButton;
    const collapseSection = dataSectionElement.$.collapse;
    assertTrue(isVisible(expandButton));
    assertFalse(expandButton.expanded);
    assertFalse(collapseSection.opened);
    expandButton.click();
    await microtasksFinished();
    assertTrue(expandButton.expanded);
    assertTrue(collapseSection.opened);

    // Unselect the first checkbox.
    const checkboxes =
        dataSectionElement.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(checkboxes.length, TEST_DATA.dataItems.length);
    const firstCheckbox = checkboxes[0]!;
    assertTrue(firstCheckbox.checked);
    firstCheckbox.click();
    await microtasksFinished();
    assertFalse(firstCheckbox.checked);
    // Output should contain only the second element.
    assertDeepEquals(new Set<number>([2]), dataSectionElement.dataSelected);

    // Uncheck toggle.
    const toggle = dataSectionElement.$.toggle;
    assertTrue(toggle.checked);
    toggle.click();
    await microtasksFinished();
    assertFalse(toggle.checked);
    // First checkbox should be still off.
    assertFalse(firstCheckbox.checked);
    // Expand button hidden.
    assertFalse(isVisible(expandButton));
    // Output should be empty.
    assertDeepEquals(new Set<number>(), dataSectionElement.dataSelected);

    // Check toggle again
    toggle.click();
    await microtasksFinished();
    // Everything in the section should be reinitialized, despite changes to the
    // initial checkbox value or collapse/expandButton elements.
    assertTrue(firstCheckbox.checked);
    assertTrue(isVisible(expandButton));
    assertFalse(expandButton.expanded);
    assertFalse(collapseSection.opened);
    // Output should contain all elements.
    assertDeepEquals(new Set<number>([1, 2]), dataSectionElement.dataSelected);
  });

  test('UnckeckingAllCheckboxesShouldDisableSection', async function() {
    assertTrue(isVisible(dataSectionElement));
    await microtasksFinished();

    // Unselect the first checkbox.
    const checkboxes =
        dataSectionElement.shadowRoot!.querySelectorAll<CrCheckboxElement>(
            '.item-checkbox');
    assertEquals(checkboxes.length, TEST_DATA.dataItems.length);

    const toggle = dataSectionElement.$.toggle;
    assertTrue(toggle.checked);

    for (let i = 0; i < checkboxes.length; ++i) {
      const checkbox = checkboxes[i]!;
      assertTrue(checkbox.checked);
      checkbox.click();
    }
    await microtasksFinished();

    // Section is disabled and output has no items.
    assertFalse(toggle.checked);
    assertDeepEquals(new Set<number>(), dataSectionElement.dataSelected);

    // Enable toggle again.
    toggle.click();
    await microtasksFinished();

    // Checkboxes are checked
    for (let i = 0; i < checkboxes.length; ++i) {
      assertTrue(checkboxes[i]!.checked);
    }
    // Output has all items.
    assertDeepEquals(new Set<number>([1, 2]), dataSectionElement.dataSelected);
  });
});
