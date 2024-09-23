// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';

import type {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr_tabs_test', function() {
  let tabs: CrTabsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tabs = document.createElement('cr-tabs');
    tabs.tabNames = ['tab1', 'tab2', 'tab3'];
    tabs.tabIcons = ['chrome://icon1.png'];
    document.body.appendChild(tabs);
  });

  function getTabElement(index: number): HTMLElement {
    return tabs.shadowRoot!.querySelector(`.tab:nth-of-type(${index + 1})`)!;
  }

  async function checkUiChange(
      uiChange: Function, initialSelection: number, expectedSelection: number) {
    tabs.selected = initialSelection;
    await tabs.updateComplete;
    if (initialSelection === expectedSelection) {
      uiChange();
    } else {
      const wait = eventToPromise('selected-changed', tabs);
      uiChange();
      await wait;
    }
    // Wait for updateComplete here, to allow any errors in the update cycle to
    // surface (required for the 'initial tab out of bound' test to properly
    // fail if errors occur in the update cycle).
    await tabs.updateComplete;
    assertEquals(expectedSelection, tabs.selected);
    const tabElement = getTabElement(expectedSelection);
    assertTrue(!!tabElement);
    assertTrue(tabElement.classList.contains('selected'));
    assertEquals('0', tabElement.getAttribute('tabindex'));
    const notSelected =
        tabs.shadowRoot!.querySelectorAll('.tab:not(.selected)');
    assertEquals(2, notSelected.length);
    notSelected.forEach(tab => {
      assertEquals('-1', tab.getAttribute('tabindex'));
    });
  }

  function checkKey(
      key: string, initialSelection: number, expectedSelection: number) {
    return checkUiChange(
        () => keyDownOn(tabs, 0, [], key), initialSelection, expectedSelection);
  }

  function checkClickTab(initialSelection: number, expectedSelection: number) {
    return checkUiChange(
        () => getTabElement(expectedSelection).click(), initialSelection,
        expectedSelection);
  }

  function checkClickTabChild(
      initialSelection: number, expectedSelection: number) {
    return checkUiChange(() => {
      const tabIndicator = getTabElement(expectedSelection)
                               .querySelector<HTMLElement>('.tab-indicator');
      assertTrue(!!tabIndicator);
      tabIndicator.click();
    }, initialSelection, expectedSelection);
  }

  test('check CSS classes, aria-selected and tabindex for a tab', async () => {
    const tab = getTabElement(0);
    assertEquals(1, tab.classList.length);
    assertEquals('false', tab.getAttribute('aria-selected'));
    assertEquals('-1', tab.getAttribute('tabindex'));

    tabs.selected = 0;
    await tabs.updateComplete;
    assertEquals(2, tab.classList.length);
    assertTrue(tab.classList.contains('selected'));
    assertEquals('true', tab.getAttribute('aria-selected'));
    assertEquals('0', tab.getAttribute('tabindex'));

    tabs.selected = 1;
    await tabs.updateComplete;
    assertEquals(1, tab.classList.length);
    assertEquals('false', tab.getAttribute('aria-selected'));
    assertEquals('-1', tab.getAttribute('tabindex'));
  });

  test('tab icons are optional', () => {
    const tab0 = getTabElement(0);
    const tabIcon0 = tab0.querySelector('.tab-icon')!;
    assertNotEquals('none', getComputedStyle(tabIcon0).display);

    const tab1 = getTabElement(1);
    const tabIcon1 = tab1.querySelector('.tab-icon')!;
    assertEquals('none', getComputedStyle(tabIcon1).display);
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

  test(
      'clicking on tabs children, selection changes and event fires',
      async () => {
        await checkClickTabChild(0, 0);
        await checkClickTabChild(1, 0);
        await checkClickTabChild(2, 0);
        await checkClickTabChild(0, 1);
        await checkClickTabChild(1, 1);
        await checkClickTabChild(2, 1);
        await checkClickTabChild(0, 2);
        await checkClickTabChild(1, 2);
        await checkClickTabChild(2, 2);
      });

  test('initial tab out of bound', async () => {
    // When old selected tab is out of bound, onSelectedChanged_ should early
    // return, rather than trigger out of bound error.
    await checkUiChange(
        () => getTabElement(1).click(), /*initialSelection=*/ 10,
        /*expectedSelection=*/ 1);
  });
});
