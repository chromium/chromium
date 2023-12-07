// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/shortcuts_page.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/js/accelerator_row.js';
import {createFakeMojoAccelInfo, createFakeMojoLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {AcceleratorCategory, AcceleratorSource, AcceleratorSubcategory, MojoAcceleratorConfig, MojoLayoutInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {SHORTCUTS_APP_URL} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {ShortcutsPageElement} from 'chrome://shortcut-customization/js/shortcuts_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

function initShortcutsPageElement(category: AcceleratorCategory):
    ShortcutsPageElement {
  const element =
      document.createElement('shortcuts-page') as ShortcutsPageElement;
  element.initialData = {category};
  document.body.appendChild(element);
  flush();
  return element;
}

/**
 * Initialize the AcceleratorLookupManager, and create fake accelerators equal
 * to `numFakeAccelerators`, with those accelerators split between two
 * subcategories based on the value of `numFirstSubCategoryAccelerators`.
 * @param numFakeAccelerators the number of fake accelerators to create.
 * @param numFirstSubCategoryAccelerators the number of accelerators in the
 *     first subcategory; the rest will be in the second subcategory. Should not
 *     exceed the value of `numFakeAccelerators`.
 * @returns the AcceleratorLookupManager instance.
 */
function initManagerWithFakeData(
    numFakeAccelerators: number,
    numFirstSubCategoryAccelerators: number): AcceleratorLookupManager {
  assert(numFirstSubCategoryAccelerators <= numFakeAccelerators);
  const manager = AcceleratorLookupManager.getInstance();
  const acceleratorConfig:
      MojoAcceleratorConfig = {[AcceleratorSource.kAsh]: {}};
  for (let i = 0; i < numFakeAccelerators; i++) {
    acceleratorConfig[AcceleratorCategory.kGeneral]![i] =
        [createFakeMojoAccelInfo()];
  }
  manager.setAcceleratorLookup(acceleratorConfig);

  const blankLayoutInfos =
      Array<MojoLayoutInfo>(numFakeAccelerators)
          .fill(createFakeMojoLayoutInfo(
              'description', /*action=*/ 0, AcceleratorCategory.kGeneral,
              AcceleratorSource.kAsh));
  const layoutInfos = blankLayoutInfos.map((layoutInfo, index) => {
    return {
      ...layoutInfo,
      subCategory: index < numFirstSubCategoryAccelerators ?
          AcceleratorSubcategory.kApps :
          AcceleratorSubcategory.kGeneralControls,
      action: index,
    };
  });
  manager.setAcceleratorLayoutLookup(layoutInfos);
  return manager;
}

/**
 * Check if any part of the given `element` is visible within the viewport.
 * Does not account for horizontal visibility.
 * @param element The element to check for visibility.
 * @returns True if any part of the element is visible within the viewport.
 */
function isVisibleVerticallyInViewport(element: HTMLElement): boolean {
  const rect = element.getBoundingClientRect();
  return rect.top < window.innerHeight && rect.bottom >= 0;
}

/**
 * Override the specified function and parameters for the given class to avoid
 * scroll animations that delay element property updates.
 *
 * This function has been adapted from `disableAnimationBehavior` in
 * chrome/test/data/webui/tab_search/tab_search_test_helper.ts.
 */
function disableScrollAnimation(element: any) {
  const originalFunction = element.prototype['scrollIntoView'];
  element.prototype['scrollIntoView'] = function(options: any) {
    const args = [];
    if (typeof options === 'object' && options !== null) {
      const noAnimationOptions = Object.assign({}, options);
      // Delete the `behavior` property from the args, since that's the property
      // that enables smooth scrolling.
      delete noAnimationOptions.behavior;

      args.push(noAnimationOptions);
    }
    originalFunction.apply(this, args);
  };
}

suite('ShortcutsPageTest', function() {
  let shortcutsPageElement: ShortcutsPageElement|null = null;

  let manager: AcceleratorLookupManager|null = null;

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    if (shortcutsPageElement) {
      shortcutsPageElement.remove();
    }
    shortcutsPageElement = null;

    // Reset the scroll position to the top of the page since the tests depend
    // on it.
    window.scrollTo({top: 0});
  });

  test('AcceleratorSubsections render correctly', () => {
    const numFakeAccelerators = 10;
    const numFirstCategoryAccelerators = 4;
    manager = initManagerWithFakeData(
        numFakeAccelerators, numFirstCategoryAccelerators);

    shortcutsPageElement =
        initShortcutsPageElement(AcceleratorCategory.kGeneral);
    flush();
    const subsections = shortcutsPageElement.shadowRoot?.querySelectorAll(
        'accelerator-subsection');
    assertTrue(!!subsections);
    assertEquals(2, subsections.length);

    const rowsForFirstSubsection =
        subsections.item(0).shadowRoot?.querySelectorAll('accelerator-row');
    assertTrue(!!rowsForFirstSubsection);
    assertEquals(numFirstCategoryAccelerators, rowsForFirstSubsection.length);

    const rowsForSecondSubsection =
        subsections.item(1).shadowRoot?.querySelectorAll('accelerator-row');
    assertTrue(!!rowsForSecondSubsection);
    assertEquals(
        numFakeAccelerators - numFirstCategoryAccelerators,
        rowsForSecondSubsection.length);
  });

  test('ScrollIntoView works when route changes', async () => {
    // Create enough accelerators so that the last accelerator is out of view.
    const numFakeAccelerators = 40;
    const numFirstCategoryAccelerators = 10;
    manager = initManagerWithFakeData(
        numFakeAccelerators, numFirstCategoryAccelerators);

    shortcutsPageElement =
        initShortcutsPageElement(AcceleratorCategory.kGeneral);
    flush();
    const subsections = shortcutsPageElement.shadowRoot?.querySelectorAll(
        'accelerator-subsection');
    assertTrue(!!subsections);

    const rowsForSecondSubsection =
        subsections.item(1).shadowRoot?.querySelectorAll('accelerator-row');
    assertTrue(!!rowsForSecondSubsection);
    const lastAcceleratorRow =
        rowsForSecondSubsection.item(rowsForSecondSubsection.length - 1);

    // The AcceleratorRow is at the bottom of the page, so it's hidden.
    assertFalse(isVisibleVerticallyInViewport(lastAcceleratorRow));

    // Disable smooth scroll so that the scroll completes instantly.
    disableScrollAnimation(AcceleratorRowElement);
    shortcutsPageElement.setScrollTimeoutForTesting(/*timeout=*/ 0);

    // Trigger onRouteChanged as if the user had selected a SearchResultRow.
    shortcutsPageElement.onRouteChanged(new URL(`${SHORTCUTS_APP_URL}?action=${
        lastAcceleratorRow.action}&category=${AcceleratorCategory.kGeneral}`));

    await flushTasks();
    // After `onRouteChanged`, the AcceleratorRow is now visible.
    assertTrue(isVisibleVerticallyInViewport(lastAcceleratorRow));
    // TODO(longbowei): Add test to verify lastAcceleratorRow is focused.
  });

  test('ScrollIntoView works when page changes', async () => {
    // Create enough accelerators so that the last accelerator is out of view.
    const numFakeAccelerators = 40;
    manager = initManagerWithFakeData(numFakeAccelerators, 10);

    shortcutsPageElement =
        initShortcutsPageElement(AcceleratorCategory.kGeneral);
    flush();
    const subsections = shortcutsPageElement.shadowRoot?.querySelectorAll(
        'accelerator-subsection');
    assertTrue(!!subsections);

    const rowsForSecondSubsection =
        subsections.item(1).shadowRoot?.querySelectorAll('accelerator-row');
    assertTrue(!!rowsForSecondSubsection);
    const lastAcceleratorRow =
        rowsForSecondSubsection.item(rowsForSecondSubsection.length - 1);

    // The AcceleratorRow is at the bottom of the page, so it's hidden.
    assertFalse(isVisibleVerticallyInViewport(lastAcceleratorRow));

    // Disable smooth scroll so that the scroll completes instantly.
    disableScrollAnimation(AcceleratorRowElement);
    shortcutsPageElement.setScrollTimeoutForTesting(/*timeout=*/ 0);

    // Update the URL of the app and trigger onNavigationPageChanged as if the
    // user had selected a SearchResultRow.
    window.history.pushState(
        {}, '',
        `${SHORTCUTS_APP_URL}?action=${lastAcceleratorRow.action}&category=${
            AcceleratorCategory.kGeneral}`);
    shortcutsPageElement.onNavigationPageChanged({isActive: true});

    await waitAfterNextRender(shortcutsPageElement);

    // After `onNavigationPageChanged`, the AcceleratorRow is now
    // visible.
    assertTrue(isVisibleVerticallyInViewport(lastAcceleratorRow));
    // TODO(longbowei): Add test to verify lastAcceleratorRow is focused.
  });
});
