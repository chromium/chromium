// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {CriticalAction, HistoryEntry, HistoryItemElement, HistoryListElement} from 'chrome://history/history.js';
import {BrowserProxyImpl, CriticalActionType} from 'chrome://history/history.js';
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
  let testProxy: TestHistoryBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestHistoryBrowserProxy();
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

    const expectedCriticalActions: CriticalAction[] = [
      {
        id: 'phone',
        label: 'Phone number filled',
        tooltip: 'Contact info',
        linkoutUrl: 'chrome://settings/addresses',
        actionType: CriticalActionType.kFormFill,
      },
      {
        id: 'email',
        label: 'Email filled',
        tooltip: 'Contact info',
        linkoutUrl: 'chrome://settings/addresses',
        actionType: CriticalActionType.kFormFill,
      },
      {
        id: 'payment',
        label: 'Payment method filled',
        tooltip: 'Payment methods',
        linkoutUrl: 'chrome://settings/payments',
        actionType: CriticalActionType.kFormFill,
      },
    ];

    const newResults = [...TEST_HISTORY_RESULTS];
    newResults[1]!.isActorVisit = true;
    newResults[1]!.criticalActions = expectedCriticalActions;
    element.addNewResults(newResults, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    const startActorIcon = items[1]!.shadowRoot.querySelector<HTMLElement>(
        '#title-and-domain #actor-icon');
    const endActorIcon =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#icons #actor-icon');
    assertTrue(isVisible(startActorIcon));
    assertEquals('CR-ICON', startActorIcon!.tagName);
    assertFalse(isVisible(endActorIcon));

    // Verify expand button for actor visit item.
    const nonActorExpandBtn =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
    assertFalse(isVisible(nonActorExpandBtn));

    const actorExpandBtn =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
    assertTrue(isVisible(actorExpandBtn));
    assertEquals(
        'cr:keyboard-arrow-down', actorExpandBtn!.getAttribute('iron-icon'));
    const menuBtn = items[1]!.$.menuButton;
    assertEquals(
        menuBtn.getAttribute('aria-describedby'),
        actorExpandBtn!.getAttribute('aria-describedby'));

    const collapse =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#collapse');
    assertTrue(!!collapse);
    assertFalse(collapse.hasAttribute('opened'));
    const timeGapSeparator =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#time-gap-separator');
    assertTrue(isVisible(timeGapSeparator));

    actorExpandBtn!.click();
    await microtasksFinished();

    assertEquals(
        'cr:keyboard-arrow-up', actorExpandBtn!.getAttribute('iron-icon'));
    assertTrue(collapse.hasAttribute('opened'));
    assertFalse(isVisible(timeGapSeparator));
    assertEquals(1, testProxy.actionMap['HistoryPage_CriticalActionsExpanded']);

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

    const actionRows = items[1]!.shadowRoot.querySelectorAll<HTMLElement>(
        '.critical-action-row');
    assertEquals(expectedCriticalActions.length, actionRows.length);

    actionRows.forEach((row, i) => {
      const expectedAction = expectedCriticalActions[i]!;
      assertEquals('listitem', row.getAttribute('role'));
      assertEquals('critical-action', row.getAttribute('focus-type'));
      assertEquals(expectedAction.label, row.getAttribute('aria-label'));
      assertEquals(
          `critical-action-icon-${i}`, row.getAttribute('aria-describedby'));

      const label = row.querySelector('.critical-action-label');
      assertTrue(!!label);
      assertEquals(expectedAction.label, label.textContent.trim());

      const icon = row.querySelector<HTMLElement>('.critical-action-button');
      assertTrue(!!icon);
      assertEquals('CR-ICON', icon.tagName);
      assertEquals(`critical-action-icon-${i}`, icon.id);
      assertEquals('cr:open-in-new', icon.getAttribute('icon'));
      assertEquals('img', icon.getAttribute('role'));
      assertEquals(expectedAction.tooltip, icon.getAttribute('title'));
      assertEquals(
          `${expectedAction.tooltip}, opens in a new tab`,
          icon.getAttribute('aria-label'));
    });

    let openedUrl = '';
    const originalOpen = window.open;
    try {
      window.open = (url) => {
        openedUrl = url as string;
        return null;
      };
      actionRows[0]!.click();
      assertEquals(expectedCriticalActions[0]!.linkoutUrl, openedUrl);
      assertEquals(
          1,
          testProxy.histogramMap['HistoryPage.CriticalAction.Click']!
              [CriticalActionType.kFormFill]);
    } finally {
      window.open = originalOpen;
    }

    // Collapsing the item should restore the time gap separator.
    actorExpandBtn!.click();
    await microtasksFinished();
    assertFalse(collapse.hasAttribute('opened'));
    assertTrue(isVisible(timeGapSeparator));
    assertEquals(
        1, testProxy.actionMap['HistoryPage_CriticalActionsCollapsed']);
  });

  test(
      'actor visit with WebMCP critical action renders row without linkout',
      async function() {
        loadTimeData.overrideValues({
          enableBrowsingHistoryActorIntegrationM1: true,
          isCriticalActionsEnabled: true,
        });

        const expectedCriticalActions: CriticalAction[] = [
          {
            id: 'phone',
            label: 'Phone number filled',
            tooltip: 'Contact info',
            linkoutUrl: 'chrome://settings/addresses',
            actionType: CriticalActionType.kFormFill,
          },
          {
            id: 'webmcp',
            label: 'Used a WebMCP tool',
            tooltip: 'Website actions',
            linkoutUrl: null,
            actionType: CriticalActionType.kWebMcpTool,
          },
        ];

        const newResults = [...TEST_HISTORY_RESULTS];
        newResults[1]!.isActorVisit = true;
        newResults[1]!.criticalActions = expectedCriticalActions;
        element.addNewResults(newResults, false, true);
        await microtasksFinished();

        const items = element.shadowRoot.querySelectorAll('history-item');
        const actorExpandBtn =
            items[1]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
        assertTrue(isVisible(actorExpandBtn));
        actorExpandBtn!.click();
        await microtasksFinished();

        const actionRows = items[1]!.shadowRoot.querySelectorAll<HTMLElement>(
            '.critical-action-row');
        assertEquals(2, actionRows.length);

        // First row (FormFill) has linkout button and interactive attributes.
        const formFillRow = actionRows[0]!;
        assertTrue(formFillRow.classList.contains('with-linkout'));
        assertTrue(formFillRow.hasAttribute('focus-row-control'));
        assertTrue(formFillRow.hasAttribute('tabindex'));
        assertEquals('critical-action', formFillRow.getAttribute('focus-type'));
        assertEquals(
            'Phone number filled', formFillRow.getAttribute('aria-label'));
        assertEquals(
            'critical-action-icon-0',
            formFillRow.getAttribute('aria-describedby'));
        assertEquals(null, formFillRow.getAttribute('title'));
        assertTrue(!!formFillRow.querySelector('.critical-action-button'));

        // Second row (WebMcpTool) is keyboard-focusable and accessible, but
        // has no linkout button.
        const webMcpRow = actionRows[1]!;
        assertFalse(webMcpRow.classList.contains('with-linkout'));
        assertTrue(webMcpRow.hasAttribute('focus-row-control'));
        assertTrue(webMcpRow.hasAttribute('tabindex'));
        assertEquals('critical-action', webMcpRow.getAttribute('focus-type'));
        assertEquals(
            'Used a WebMCP tool', webMcpRow.getAttribute('aria-label'));
        assertEquals('', webMcpRow.getAttribute('aria-describedby'));
        assertEquals(null, webMcpRow.getAttribute('title'));
        assertEquals(
            'Used a WebMCP tool',
            webMcpRow.querySelector('.critical-action-label')!
                .textContent.trim());
        assertEquals(null, webMcpRow.querySelector('.critical-action-button'));

        let openedUrl = '';
        const originalOpen = window.open;
        try {
          window.open = (url) => {
            openedUrl = url as string;
            return null;
          };
          webMcpRow.click();
          assertEquals('', openedUrl);
          assertEquals(
              undefined,
              testProxy.histogramMap['HistoryPage.CriticalAction.Click']
                  ?.[CriticalActionType.kWebMcpTool]);
        } finally {
          window.open = originalOpen;
        }
      });

  test(
      'actor visit without critical actions has no expand button',
      async function() {
        loadTimeData.overrideValues({
          enableBrowsingHistoryActorIntegrationM1: true,
          isCriticalActionsEnabled: true,
        });

        const newResults = [...TEST_HISTORY_RESULTS];
        newResults[1]!.isActorVisit = true;
        newResults[1]!.criticalActions = [];
        element.addNewResults(newResults, false, true);
        await microtasksFinished();

        const items = element.shadowRoot.querySelectorAll('history-item');
        const expandBtn =
            items[1]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
        assertFalse(isVisible(expandBtn));
        const collapse =
            items[1]!.shadowRoot.querySelector<HTMLElement>('#collapse');
        assertFalse(isVisible(collapse));
      });

  test('non-actor visit with critical actions enabled', async function() {
    loadTimeData.overrideValues({
      enableBrowsingHistoryActorIntegrationM1: true,
      isCriticalActionsEnabled: true,
    });

    const newResults = [...TEST_HISTORY_RESULTS];
    newResults[0]!.isActorVisit = false;
    newResults[0]!.criticalActions = [];
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

  test(
      'non-actor visit with critical actions has expand button',
      async function() {
        loadTimeData.overrideValues({
          isCriticalActionsEnabled: true,
        });

        const newResults = [...TEST_HISTORY_RESULTS];
        newResults[0]!.isActorVisit = false;
        newResults[0]!.criticalActions = [
          {
            id: 'action-456',
            label: 'Password filled',
            tooltip: 'Saved passwords',
            linkoutUrl: 'chrome://password-manager',
            actionType: CriticalActionType.kCredentialAccess,
          },
        ];
        element.addNewResults(newResults, false, true);
        await microtasksFinished();

        const items = element.shadowRoot.querySelectorAll('history-item');
        const expandBtn =
            items[0]!.shadowRoot.querySelector<HTMLElement>('#expand-button');
        assertTrue(isVisible(expandBtn));
        const collapse =
            items[0]!.shadowRoot.querySelector<HTMLElement>('#collapse');
        assertTrue(!!collapse);
        assertFalse(collapse.hasAttribute('opened'));

        expandBtn!.click();
        await microtasksFinished();
        assertTrue(collapse.hasAttribute('opened'));

        const actionRows =
            items[0]!.shadowRoot.querySelectorAll('.critical-action-row');
        assertEquals(1, actionRows.length);
      });

  test('collapse and expand day card', async function() {
    element.addNewResults(TEST_HISTORY_RESULTS, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);

    assertTrue(items[0]!.isCardStart);
    assertFalse(items[0]!.isCardEnd);
    assertFalse(items[0]!.isCardCollapsed);
    assertTrue(items[4]!.isCardEnd);

    const dateAccessed =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#date-accessed')!;
    assertEquals('true', dateAccessed.getAttribute('aria-expanded'));
    const collapseBtn = items[0]!.$.cardCollapseButton;
    assertEquals('cr:keyboard-arrow-up', collapseBtn.ironIcon);
    const cardCollapse =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#card-collapse')!;
    assertTrue(cardCollapse.hasAttribute('opened'));
    assertTrue(isVisible(items[1]!));
    assertTrue(isVisible(items[4]!));

    // Collapse the 2016-03-16 card via the header collapse button.
    collapseBtn.click();
    await microtasksFinished();

    assertTrue(items[0]!.isCardCollapsed);
    assertTrue(items[0]!.isCardEnd);
    assertEquals('false', dateAccessed.getAttribute('aria-expanded'));
    assertEquals('cr:keyboard-arrow-down', collapseBtn.ironIcon);
    assertFalse(cardCollapse.hasAttribute('opened'));
    assertFalse(isVisible(
        items[0]!.shadowRoot.querySelector<HTMLElement>('#item-container')));
    assertFalse(isVisible(items[1]!));
    assertFalse(isVisible(items[2]!));
    assertFalse(isVisible(items[3]!));
    assertFalse(isVisible(items[4]!));

    // The 2016-03-15 card remains visible and expanded.
    assertFalse(items[5]!.isCardCollapsed);
    assertTrue(isVisible(items[5]!));

    // Re-expand the 2016-03-16 card by clicking the card header.
    dateAccessed.click();
    await microtasksFinished();

    assertFalse(items[0]!.isCardCollapsed);
    assertFalse(items[0]!.isCardEnd);
    assertTrue(items[4]!.isCardEnd);
    assertEquals('true', dateAccessed.getAttribute('aria-expanded'));
    assertEquals('cr:keyboard-arrow-up', collapseBtn.ironIcon);
    assertTrue(cardCollapse.hasAttribute('opened'));
    assertTrue(isVisible(items[1]!));
    assertTrue(isVisible(items[4]!));
  });

  test(
      'collapse and expand website domain group within a day',
      async function() {
    element.addNewResults(TEST_HISTORY_RESULTS, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);

    // Single-visit domains within a day do not show a domain collapse button.
    assertTrue(items[0]!.isDomainGroupStart);
    assertEquals(1, items[0]!.domainGroupSize);
    assertEquals(
        null, items[0]!.shadowRoot.querySelector('#domainCollapseButton'));
    assertTrue(items[1]!.isDomainGroupStart);
    assertEquals(1, items[1]!.domainGroupSize);
    assertEquals(
        null, items[1]!.shadowRoot.querySelector('#domainCollapseButton'));

    // Repeated visits to www.website.com on 2016-03-16 (items[3] and items[4])
    // form a collapsible domain group on items[3].
    assertTrue(items[3]!.isDomainGroupStart);
    assertEquals(2, items[3]!.domainGroupSize);
    assertFalse(items[4]!.isDomainGroupStart);
    assertEquals(0, items[4]!.domainGroupSize);

    const domainBtn = items[3]!.shadowRoot.querySelector<HTMLElement>(
        '#domainCollapseButton');
    assertTrue(!!domainBtn);
    assertTrue(isVisible(domainBtn));
    assertEquals(
        'cr:keyboard-arrow-up', domainBtn.getAttribute('iron-icon'));
    assertEquals('true', domainBtn.getAttribute('aria-expanded'));
    assertEquals(
        null, items[4]!.shadowRoot.querySelector('#domainCollapseButton'));
    assertTrue(items[3]!.hasTimeGap);
    assertFalse(items[3]!.isCardEnd);
    assertTrue(items[4]!.isCardEnd);

    // Collapse the www.website.com group.
    domainBtn.click();
    await microtasksFinished();

    assertTrue(items[3]!.isDomainGroupCollapsed);
    assertFalse(items[3]!.isDomainCollapsed);
    assertTrue(items[4]!.isDomainCollapsed);
    assertTrue(isVisible(items[3]!));
    assertFalse(isVisible(items[4]!));
    assertTrue(items[3]!.isCardEnd);
    assertFalse(items[3]!.hasTimeGap);
    assertEquals(
        'cr:keyboard-arrow-down', domainBtn.getAttribute('iron-icon'));
    assertEquals('false', domainBtn.getAttribute('aria-expanded'));

    // Re-expand the www.website.com group.
    domainBtn.click();
    await microtasksFinished();

    assertFalse(items[3]!.isDomainGroupCollapsed);
    assertFalse(items[4]!.isDomainCollapsed);
    assertTrue(isVisible(items[4]!));
    assertFalse(items[3]!.isCardEnd);
    assertTrue(items[4]!.isCardEnd);
    assertTrue(items[3]!.hasTimeGap);
  });
});
