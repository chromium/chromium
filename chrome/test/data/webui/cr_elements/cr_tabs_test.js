// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.m.js';
// #import {eventToPromise, flushTasks} from '../test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// clang-format on

suite('cr_tabs_test', function() {
  /** @type {?CrTabsElement} */
  let tabs = null;

  setup(() => {
    PolymerTest.clearBody();
    document.body.innerHTML = `<cr-tabs></cr-tabs>`;
    tabs = document.querySelector('cr-tabs');
    tabs.tabNames = ['tab1', 'tab2', 'tab3'];
    return test_util.flushTasks();
  });

  /**
   * @param {number} index
   * @return {HTMLElement}
   */
  function getTabElement(index) {
    return tabs.$$(`.tab:nth-of-type(${index + 1})`);
  }

  /**
   * @param {Function} uiChange
   * @param {number} initialSelection
   * @param {number} expectedSelection
   */
  async function checkUiChange(uiChange, initialSelection, expectedSelection) {
    tabs.selected = initialSelection;
    if (initialSelection == expectedSelection) {
      uiChange();
    } else {
      const wait = test_util.eventToPromise('selected-changed', tabs);
      uiChange();
      await wait;
    }
    assertEquals(expectedSelection, tabs.selected);
    const tabElement = getTabElement(expectedSelection);
    assertTrue(!!tabElement);
    assertTrue(tabElement.classList.contains('selected'));
    assertEquals('0', tabElement.getAttribute('tabindex'));
    assertEquals(getDeepActiveElement(), tabElement);
    const notSelected = tabs.shadowRoot.querySelectorAll('.tab:not(.selected)');
    assertEquals(2, notSelected.length);
    notSelected.forEach(tab => {
      assertEquals('-1', tab.getAttribute('tabindex'));
    });
  }

  /**
   * @param {string} string
   * @param {number} initialSelection
   * @param {number} expectedSelection
   */
  async function checkKey(key, initialSelection, expectedSelection) {
    await checkUiChange(
        () => MockInteractions.keyDownOn(tabs, null, [], key), initialSelection,
        expectedSelection);
  }

  /**
   * @param {number} initialSelection
   * @param {number} expectedSelection
   */
  async function checkClickTab(initialSelection, expectedSelection) {
    await checkUiChange(
        () => getTabElement(expectedSelection).click(), initialSelection,
        expectedSelection);
  }

  test('check CSS classes, aria-selected and tabindex for a tab', () => {
    const tab = getTabElement(0);
    assertEquals(1, tab.classList.length);
    assertEquals('false', tab.getAttribute('aria-selected'));
    assertEquals('-1', tab.getAttribute('tabindex'));
    tabs.selected = 0;
    assertEquals(2, tab.classList.length);
    assertTrue(tab.classList.contains('selected'));
    assertEquals('true', tab.getAttribute('aria-selected'));
    assertEquals('0', tab.getAttribute('tabindex'));
    tabs.selected = 1;
    assertEquals(1, tab.classList.length);
    assertEquals('false', tab.getAttribute('aria-selected'));
    assertEquals('-1', tab.getAttribute('tabindex'));
  });

  test('right/left pressed, selection changes and event fires', async () => {
    await checkKey('ArrowRight', 0, 1);
    await checkKey('ArrowRight', 1, 2);
    // Check that the selection wraps.
    await checkKey('ArrowRight', 2, 0);

    await checkKey('ArrowLeft', 2, 1);
    await checkKey('ArrowLeft', 1, 0);
    // Check that the selection wraps.
    await checkKey('ArrowLeft', 0, 2);

    await checkKey('Home', 0, 0);
    await checkKey('Home', 1, 0);
    await checkKey('Home', 2, 0);
    await checkKey('End', 0, 2);
    await checkKey('End', 1, 2);
    await checkKey('End', 2, 2);
  });

  test('clicking on tabs, selection changes and event fires', async () => {
    await checkClickTab(0, 0);
    await checkClickTab(1, 0);
    await checkClickTab(2, 0);
    await checkClickTab(0, 1);
    await checkClickTab(1, 1);
    await checkClickTab(2, 1);
    await checkClickTab(0, 2);
    await checkClickTab(1, 2);
    await checkClickTab(2, 2);
  });

  test('selection underline does not freeze with two tabs', async () => {
    const underline = tabs.$.selectionBar;
    const fullyExpanded = 'translateX(0%) scaleX(1)';
    tabs.tabNames = ['tab1', 'tab2'];
    assertEquals(undefined, tabs.selected);
    tabs.selected = 0;
    await test_util.flushTasks();
    assertNotEquals(fullyExpanded, underline.style.transform);
    underline.style.transform = fullyExpanded;
    const wait = test_util.eventToPromise('transitionend', underline);
    tabs.selected = 1;
    await wait;
  });
});
