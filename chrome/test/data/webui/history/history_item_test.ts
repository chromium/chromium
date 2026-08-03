// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {CriticalActionItem, HistoryEntry, HistoryItemElement, HistoryListElement} from 'chrome://history/history.js';
import {BrowserProxyImpl} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryEntry, createSearchEntry} from './test_util.js';

const TEST_HISTORY_RESULTS = [
  createHistoryEntry('2016-03-16 10:00', 'http://www.google.com'),
  createHistoryEntry('2016-03-16 9:00', 'http://www.example.com'),
  createHistoryEntry('2016-03-16 7:01', 'http://www.badssl.com'),
  createHistoryEntry('2016-03-16 7:00', 'http://www.website.com'),
  createHistoryEntry('2016-03-16 4:00', 'http://www.website.com'),
  createHistoryEntry('2016-03-15 11:00', 'http://www.example.com'),
];

const SEARCH_HISTORY_RESULTS = [
  createSearchEntry('2016-03-16', 'http://www.google.com'),
  createSearchEntry('2016-03-14 11:00', 'http://calendar.google.com'),
  createSearchEntry('2016-03-14 10:00', 'http://mail.google.com'),
];

suite('<history-item> unit test', function() {
  let item: HistoryItemElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(new TestHistoryBrowserProxy());

    item = document.createElement('history-item');
    item.item = TEST_HISTORY_RESULTS[0]!;
    document.body.appendChild(item);
  });

  test('click targets for selection', async function() {
    let selectionCount = 0;
    item.addEventListener('history-checkbox-select', function() {
      selectionCount++;
    });

    // Checkbox should trigger selection.
    item.$.checkbox.click();
    await microtasksFinished();
    assertEquals(1, selectionCount);

    // Non-interactive text should trigger selection.
    item.$.timeAccessed.click();
    assertEquals(2, selectionCount);

    // Menu button should not trigger selection.
    item.$.menuButton.click();
    assertEquals(2, selectionCount);
  });

  test('title changes with item', async function() {
    const time = item.$.timeAccessed;
    assertEquals('', time.title);

    time.dispatchEvent(new CustomEvent('mouseover'));
    const initialTitle = time.title;
    item.item = TEST_HISTORY_RESULTS[5]!;
    await microtasksFinished();
    time.dispatchEvent(new CustomEvent('mouseover'));
    assertNotEquals(initialTitle, time.title);
  });

  test(
      'website title margin adapts to critical actions flag', async function() {
        loadTimeData.overrideValues({isCriticalActionsEnabled: true});
        const enabledItem = document.createElement('history-item');
        document.body.appendChild(enabledItem);
        await microtasksFinished();
        assertTrue(enabledItem.hasAttribute('is-critical-actions-enabled_'));
        assertEquals(
            '24px',
            window.getComputedStyle(enabledItem)
                .getPropertyValue('--website-title-margin-start')
                .trim());

        loadTimeData.overrideValues({isCriticalActionsEnabled: false});
        const disabledItem = document.createElement('history-item');
        document.body.appendChild(disabledItem);
        await microtasksFinished();
        assertFalse(disabledItem.hasAttribute('is-critical-actions-enabled_'));
        assertEquals(
            '8px',
            window.getComputedStyle(disabledItem)
                .getPropertyValue('--website-title-margin-start')
                .trim());
      });
});

suite('<history-item> integration test', function() {
  let element: HistoryListElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);
    // Force a super tall body so that cr-lazy-list renders all items.
    document.body.style.height = '1000px';
    const app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    return Promise.all([
      testProxy.handler.whenCalled('queryHistory'),
      microtasksFinished(),
    ]);
  });

  function getHistoryData(): HistoryEntry[] {
    return (element.$.infiniteList.items || []);
  }

  test('basic separator insertion', async function() {
    element.addNewResults(TEST_HISTORY_RESULTS, false, true);
    await eventToPromise('viewport-filled', element.$.infiniteList);

    // Check that the correct number of time gaps are inserted.
    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);
    assertTrue(items[0]!.hasTimeGap);
    assertTrue(items[1]!.hasTimeGap);
    assertFalse(items[2]!.hasTimeGap);
    assertTrue(items[3]!.hasTimeGap);
    assertFalse(items[4]!.hasTimeGap);
    assertFalse(items[5]!.hasTimeGap);
  });

  test('separator insertion for search', async function() {
    element.addNewResults(SEARCH_HISTORY_RESULTS, false, true);
    element.searchedTerm = 'search';

    await microtasksFinished();
    const items = element.shadowRoot.querySelectorAll('history-item');

    assertTrue(items[0]!.hasTimeGap, '0');
    assertFalse(items[1]!.hasTimeGap, '1');
    assertFalse(items[2]!.hasTimeGap, '2');
  });

  test('separator insertion after deletion', async function() {
    element.addNewResults(TEST_HISTORY_RESULTS, false, true);
    await microtasksFinished();
    const items = element.shadowRoot.querySelectorAll('history-item');

    element.removeItemsByIndexForTesting([3]);
    await microtasksFinished();

    // Checks that a new time gap separator has been inserted.
    assertEquals(5, getHistoryData().length);
    assertTrue(items[2]!.hasTimeGap);

    element.removeItemsByIndexForTesting([3]);
    await microtasksFinished();

    // Checks time gap separator is removed.
    assertEquals(4, element.$.infiniteList.items.length);
    assertFalse(items[2]!.hasTimeGap);
  });

  test('remove bookmarks', async function() {
    const newResults = [...TEST_HISTORY_RESULTS];
    newResults[1]!.starred = true;
    newResults[5]!.starred = true;
    element.addNewResults(newResults, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);
    const star =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#bookmark-star');
    assertTrue(!!star);
    star.focus();
    star.click();
    await microtasksFinished();

    // Check that all items matching this url are unstarred.
    assertFalse(getHistoryData()[1]!.starred);
    assertFalse(getHistoryData()[5]!.starred);
  });

  test('actor-initiated visit annotation enabled', async function() {
    loadTimeData.overrideValues(
        {enableBrowsingHistoryActorIntegrationM1: true});

    const newResults = [...TEST_HISTORY_RESULTS];
    // Actor initiated history visit.
    newResults[1]!.isActorVisit = true;
    // Actor initiated history visit that is also bookmarked.
    newResults[5]!.isActorVisit = true;
    newResults[5]!.starred = true;
    element.addNewResults(newResults, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);
    assertFalse(isVisible(
        items[0]!.shadowRoot.querySelector<HTMLElement>('#actor-icon')));
    assertTrue(isVisible(
        items[1]!.shadowRoot.querySelector<HTMLElement>('#actor-icon')));
    assertFalse(isVisible(
        items[2]!.shadowRoot.querySelector<HTMLElement>('#actor-icon')));
    assertFalse(isVisible(
        items[3]!.shadowRoot.querySelector<HTMLElement>('#actor-icon')));
    assertFalse(isVisible(
        items[4]!.shadowRoot.querySelector<HTMLElement>('#actor-icon')));
    assertTrue(isVisible(
        items[5]!.shadowRoot.querySelector<HTMLElement>('#actor-icon')));
    assertTrue(isVisible(
        items[5]!.shadowRoot.querySelector<HTMLElement>('#bookmark-star')));
  });

  test('actor-initiated visit with critical actions enabled', async function() {
    loadTimeData.overrideValues({
      enableBrowsingHistoryActorIntegrationM1: true,
      isCriticalActionsEnabled: true,
    });

    const newResults = [...TEST_HISTORY_RESULTS];
    newResults[1]!.isActorVisit = true;
    element.addNewResults(newResults, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    const startActorIcon = items[1]!.shadowRoot.querySelector<HTMLElement>(
        '#title-and-domain #actor-icon');
    const endActorIcon =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#icons #actor-icon');
    assertTrue(isVisible(startActorIcon));
    assertFalse(isVisible(endActorIcon));

    // Verify expand button for actor visit item.
    const nonActorExpandBtn =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
    assertFalse(isVisible(nonActorExpandBtn));

    const actorExpandBtn =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
    assertTrue(isVisible(actorExpandBtn));

    const collapse =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#collapse');
    assertTrue(!!collapse);
    assertFalse(collapse.hasAttribute('opened'));

    actorExpandBtn!.click();
    await microtasksFinished();

    assertTrue(collapse.hasAttribute('opened'));

    const criticalActionsTitle =
        items[1]!.shadowRoot.querySelector<HTMLElement>(
            '.critical-actions-title');
    assertTrue(!!criticalActionsTitle);
    assertTrue(isVisible(criticalActionsTitle));
    assertEquals(
        loadTimeData.getString('geminiKeyBrowsingActionsTitle'),
        criticalActionsTitle.textContent.trim());

    const actionsList = items[1]!.shadowRoot.querySelector<HTMLElement>(
        '.critical-actions-list');
    assertTrue(!!actionsList);
    assertEquals('list', actionsList.getAttribute('role'));

    const expectedCriticalActions: CriticalActionItem[] = [
      {
        id: 'phone',
        label: 'Phone number filled',
        tooltip: 'Contact info',
        url: 'http://www.example.com',
        ariaLabel: 'Phone number filled, Contact info',
      },
      {
        id: 'email',
        label: 'Email filled',
        tooltip: 'Contact info',
        url: 'http://www.example.com',
        ariaLabel: 'Email filled, Contact info',
      },
      {
        id: 'payment',
        label: 'Payment method filled',
        tooltip: 'Payment methods',
        url: 'http://www.example.com',
        ariaLabel: 'Payment method filled, Payment methods',
      },
    ];

    const actionRows = items[1]!.shadowRoot.querySelectorAll<HTMLElement>(
        '.critical-action-row');
    assertEquals(expectedCriticalActions.length, actionRows.length);

    actionRows.forEach((row, i) => {
      const expectedAction = expectedCriticalActions[i]!;
      assertEquals('listitem', row.getAttribute('role'));
      assertEquals('critical-action', row.getAttribute('focus-type'));
      assertEquals(expectedAction.ariaLabel, row.getAttribute('aria-label'));

      const label = row.querySelector('.critical-action-label');
      assertTrue(!!label);
      assertEquals(expectedAction.label, label.textContent.trim());

      const button = row.querySelector<HTMLElement>('.critical-action-button');
      assertTrue(!!button);
      assertEquals('cr:open-in-new', button.getAttribute('iron-icon'));
      assertEquals(expectedAction.tooltip, button.getAttribute('title'));
      assertEquals(expectedAction.tooltip, button.getAttribute('aria-label'));
    });

    let openedUrl = '';
    const originalOpen = window.open;
    window.open = (url) => {
      openedUrl = url as string;
      return null;
    };
    actionRows[0]!.click();
    assertEquals(expectedCriticalActions[0]!.url, openedUrl);
    window.open = originalOpen;
  });

  test('non-actor visit with critical actions enabled', async function() {
    loadTimeData.overrideValues({
      enableBrowsingHistoryActorIntegrationM1: true,
      isCriticalActionsEnabled: true,
    });

    const newResults = [...TEST_HISTORY_RESULTS];
    newResults[0]!.isActorVisit = false;
    element.addNewResults(newResults, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    const expandBtn =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
    assertFalse(isVisible(expandBtn));
    const collapse =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#collapse');
    assertFalse(isVisible(collapse));
  });
});
