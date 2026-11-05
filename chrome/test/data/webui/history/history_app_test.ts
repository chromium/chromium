// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://history/history.js';

import type {HistoryAppElement, HistoryFilterChipsElement} from 'chrome://history/history.js';
import {BrowserProxyImpl, CrRouter} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {TimePeriodOptionId} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
// <if expr="not is_chromeos">
import {AccessPoint} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {HistorySignInState, SyncState} from 'chrome://history/history.js';

// </if>

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryInfo} from './test_util.js';

// clang-format on

suite('HistoryAppTest', function() {
  let element: HistoryAppElement;
  let browserProxy: TestHistoryBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    // Some of the tests below assume the query state is fully reset to empty
    // between tests.
    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  });

  test('SetsScrollTarget', async () => {
    assertEquals(
        element.$.tabsScrollContainer, element.getScrollTargetForTesting());

    // 'By group' view shares the same scroll container as default history view.
    element.$.router.selectedPage = 'grouped';
    await microtasksFinished();
    assertEquals(
        element.$.tabsScrollContainer, element.getScrollTargetForTesting());

    // Switching to synced tabs should change scroll target to it.
    element.$.router.selectedPage = 'syncedTabs';
    await microtasksFinished();
    assertEquals(
        element.shadowRoot.querySelector('#syncedDevicesScroll'),
        element.getScrollTargetForTesting());
  });

  // <if expr="not is_android">
  test('AndroidStylesNotApplied', () => {
    assertFalse(
        getComputedStyle(element).fontFamily.includes('Google Sans Text'));
  });
  // </if>
});

// <if expr="not is_chromeos">
// history sync promo is not shown for ChromeOS.
suite('HistoryAppUnoPhase2FollowUpTest', () => {
  let element: HistoryAppElement;
  let browserProxy: TestHistoryBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      unoPhase2FollowUp: true,
    });
    browserProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    browserProxy.handler.setResultFor(
        'shouldShowHistoryPageHistorySyncPromo', Promise.resolve({
          shouldShow: true,
        }));

    // Some of the tests below assume the query state is fully reset to empty
    // between tests.
    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();

    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  });

  test('ShowsHistorySyncPromoElementWhenDataIsTrue', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();

    assertTrue(
        isChildVisible(element, 'history-sync-promo'), 'Promo should be shown');
  });

  test('HidesHistorySyncPromoElementWhenDataIsFalse', async () => {
    browserProxy.handler.setResultFor(
        'shouldShowHistoryPageHistorySyncPromo',
        Promise.resolve({shouldShow: false}));
    // Re-create the element to pick up the new loadTimeData.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('history-app');
    document.body.appendChild(element);
    await microtasksFinished();

    assertFalse(
        isChildVisible(element, 'history-sync-promo'),
        'Promo should not be shown');
  });

  test('HidesHistorySyncPromoElementWhenHistorySyncIsDisabled', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.DISABLED,
    });
    await microtasksFinished();

    assertFalse(
        isChildVisible(element, 'history-sync-promo'),
        'Promo should not be shown');
  });

  test('HistorySyncPromoElementSignedIn', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));

    const button = historySyncPromo.shadowRoot.querySelector<HTMLElement>(
        '#sync-history-button');
    assertTrue(!!button);
    button.click();
    const accessPoint =
        await browserProxy.handler.whenCalled('turnOnHistorySync');
    assertEquals(AccessPoint.kHistoryPage, accessPoint);
  });

  test('HistorySyncPromoElementPendingSignInWithHistorySyncOn', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGN_IN_PENDING,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_ON,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(
        historySyncPromo, '#sign-in-pending-syncing-history-description'));
    assertTrue(isChildVisible(historySyncPromo, '#verify-its-you-button'));
    assertEquals(1, browserProxy.getCallCount('recordSigninPendingOffered'));
    assertEquals(
        AccessPoint.kHistoryPage,
        await browserProxy.whenCalled('recordSigninPendingOffered'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(historySyncPromo, '#sync-history-button'));
  });

  test('HistorySyncPromoElementPendingSignInWithHistorySyncOff', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGN_IN_PENDING,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });

  test('HistorySyncPromoElementWebOnlySignIn', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.WEB_ONLY_SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(
        isChildVisible(historySyncPromo, '#web-only-signed-in-description'));
    assertEquals(
        loadTimeData.getString('historySyncPromoBodyWebOnlySignedIn'),
        historySyncPromo.shadowRoot
            .querySelector<HTMLElement>(
                '#web-only-signed-in-description')!.textContent.trim());
    assertTrue(isChildVisible(historySyncPromo, '#profile-info-row'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });

  test('HistorySyncPromoElementSignedOut', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_OUT,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertEquals(
        loadTimeData.getString('historySyncPromoBodySignedOut'),
        historySyncPromo.shadowRoot
            .querySelector<HTMLElement>(
                '#signed-out-description')!.textContent.trim());
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });
});
// </if>

suite('HistoryFilterChipsVisibility', function() {
  let element: HistoryAppElement;
  let browserProxy: TestHistoryBrowserProxy;

  setup(() => {
    browserProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    // Some of the tests below assume the query state is fully reset to empty
    // between tests.
    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createPage() {
    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  }

  test('FilterChipsNotVisible_AllFiltersOff', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: false,
      isBrowsingHistoryFilterByDeviceEnabled: false,
      isBrowsingHistoryFilterByTimePeriodEnabled: false,
    });
    browserProxy.handler.setResultFor(
        'getSyncedDevicesForFiltering', Promise.resolve({
          devices: [
            {name: 'Device 1', isLocalDevice: true, ids: ['id1']},
            {name: 'Device 2', isLocalDevice: false, ids: ['id2']},
          ],
        }));
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertFalse(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsVisible_ActorFilterOnly', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: true,
      isBrowsingHistoryFilterByDeviceEnabled: false,
      isBrowsingHistoryFilterByTimePeriodEnabled: false,
    });
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertTrue(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsVisible_DeviceFilterOnly', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: false,
      isBrowsingHistoryFilterByDeviceEnabled: true,
      isBrowsingHistoryFilterByTimePeriodEnabled: false,
    });
    browserProxy.handler.setResultFor(
        'getSyncedDevicesForFiltering', Promise.resolve({
          devices: [
            {name: 'Device 1', isLocalDevice: true, ids: ['id1']},
            {name: 'Device 2', isLocalDevice: false, ids: ['id2']},
          ],
        }));
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertEquals(
        1, browserProxy.handler.getCallCount('getSyncedDevicesForFiltering'));
    assertTrue(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsNotVisible_SingleDevice', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: false,
      isBrowsingHistoryFilterByDeviceEnabled: true,
      isBrowsingHistoryFilterByTimePeriodEnabled: false,
    });
    browserProxy.handler.setResultFor(
        'getSyncedDevicesForFiltering', Promise.resolve({
          devices: [{name: 'Device 1', isLocalDevice: true, ids: ['id1']}],
        }));
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertEquals(
        1, browserProxy.handler.getCallCount('getSyncedDevicesForFiltering'));
    assertFalse(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsVisible_TimePeriodFilterOnly', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: false,
      isBrowsingHistoryFilterByDeviceEnabled: false,
      isBrowsingHistoryFilterByTimePeriodEnabled: true,
    });
    browserProxy.handler.setResultFor('getTimePeriodOptions', Promise.resolve({
      options: [{
        id: TimePeriodOptionId.kYesterday,
        label: 'Yesterday',
        beginTime: {internalValue: BigInt(0)},
        endTime: {internalValue: BigInt(1)},
      }],
    }));
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    await browserProxy.handler.whenCalled('getTimePeriodOptions');
    await microtasksFinished();
    assertTrue(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsNotVisible_TimePeriodFilterDisabled', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: false,
      isBrowsingHistoryFilterByDeviceEnabled: false,
      isBrowsingHistoryFilterByTimePeriodEnabled: false,
    });
    browserProxy.handler.setResultFor('getTimePeriodOptions', Promise.resolve({
      options: [{
        id: TimePeriodOptionId.kYesterday,
        label: 'Yesterday',
        beginTime: {internalValue: BigInt(0)},
        endTime: {internalValue: BigInt(1)},
      }],
    }));
    await createPage();
    await browserProxy.handler.whenCalled('getTimePeriodOptions');
    await microtasksFinished();
    assertFalse(isChildVisible(element, '#historyFilterChips'));
  });

  test('PropagatesFilterChanges', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: true,
      isBrowsingHistoryFilterByDeviceEnabled: true,
      isBrowsingHistoryFilterByTimePeriodEnabled: false,
    });
    await createPage();

    const filterChips =
        element.shadowRoot.querySelector<HTMLElement>('#historyFilterChips');
    assertTrue(!!filterChips);

    const changeQueryEventPromise = eventToPromise<CustomEvent<{
      search: string,
      includeUserVisits: boolean,
      includeActorVisits: boolean,
      clientIds: string[],
      timePeriodOptionId: TimePeriodOptionId,
    }>>('change-query', element);

    filterChips.dispatchEvent(new CustomEvent('filter-changed', {
      detail: {
        userVisits: false,
        actorVisits: true,
        clientIds: ['device_1', 'device_2'],
        timePeriodOptionId: TimePeriodOptionId.kYesterday,
      },
      bubbles: true,
      composed: true,
    }));

    const event = await changeQueryEventPromise;

    // The query sent to the manager includes the new filters.
    assertFalse(event.detail.includeUserVisits);
    assertTrue(event.detail.includeActorVisits);
    assertDeepEquals(['device_1', 'device_2'], event.detail.clientIds);
    assertEquals(
        TimePeriodOptionId.kYesterday, event.detail.timePeriodOptionId);
    assertEquals('', event.detail.search);
  });

  test('TimePeriodSelectionIsPassedToQuery', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: false,
      isBrowsingHistoryFilterByDeviceEnabled: false,
      isBrowsingHistoryFilterByTimePeriodEnabled: true,
    });
    browserProxy.handler.setResultFor('getTimePeriodOptions', Promise.resolve({
      options: [
        {
          id: TimePeriodOptionId.kAllTime,
          label: 'All time',
          beginTime: null,
          endTime: null,
        },
        {
          id: TimePeriodOptionId.kYesterday,
          label: 'Yesterday',
          beginTime: {internalValue: BigInt(1000)},
          endTime: {internalValue: BigInt(2000)},
        },
      ],
    }));
    await createPage();
    await browserProxy.handler.whenCalled('getTimePeriodOptions');
    await microtasksFinished();

    const filterChips =
        element.shadowRoot.querySelector<HistoryFilterChipsElement>(
            '#historyFilterChips');
    assertTrue(!!filterChips);
    const select = filterChips.shadowRoot.querySelector<HTMLSelectElement>(
        '#timePeriodSelect');
    assertTrue(!!select);

    browserProxy.handler.resetResolver('queryHistory');
    browserProxy.handler.setResultFor('queryHistory', Promise.resolve({
      results: {info: createHistoryInfo(''), value: []},
    }));
    select.value = String(TimePeriodOptionId.kYesterday);
    select.dispatchEvent(new Event('change'));

    // Verify beginTime and endTime match the selected time period.
    const args = await browserProxy.handler.whenCalled('queryHistory');
    assertDeepEquals({internalValue: BigInt(1000)}, args[2]);  // beginTime
    assertDeepEquals({internalValue: BigInt(2000)}, args[3]);  // endTime
  });

  test('HidesFilterChipsOnGroupedView', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: true,
      isBrowsingHistoryFilterByDeviceEnabled: true,
      isBrowsingHistoryFilterByTimePeriodEnabled: true,
    });
    browserProxy.handler.setResultFor(
        'getSyncedDevicesForFiltering', Promise.resolve({
          devices: [
            {name: 'Device 1', isLocalDevice: true, ids: ['id1']},
            {name: 'Device 2', isLocalDevice: false, ids: ['id2']},
          ],
        }));
    await createPage();

    assertTrue(isChildVisible(element, '#historyFilterChips'));

    element.$.router.selectedPage = 'grouped';
    await microtasksFinished();

    // Should be hidden for grouped view.
    assertFalse(isChildVisible(element, '#historyFilterChips'));

    element.$.router.selectedPage = 'history';
    await microtasksFinished();
    assertTrue(isChildVisible(element, '#historyFilterChips'));
  });

  test('PreservesFilterChipsStateAfterGroupedView', async () => {
    loadTimeData.overrideValues({
      isGlicWebActuationAvailable: true,
      isBrowsingHistoryFilterByDeviceEnabled: true,
    });
    browserProxy.handler.setResultFor(
        'getSyncedDevicesForFiltering', Promise.resolve({
          devices: [
            {name: 'Device 1', isLocalDevice: true, ids: ['id1']},
            {name: 'Device 2', isLocalDevice: false, ids: ['id2', 'id3']},
          ],
        }));
    await createPage();

    let filterChips =
        element.shadowRoot.querySelector<HistoryFilterChipsElement>(
            '#historyFilterChips');
    assertTrue(!!filterChips);
    filterChips.fire('filter-changed', {
      userVisits: false,
      actorVisits: true,
      clientIds: ['id2', 'id3'],
      timePeriodOptionId: TimePeriodOptionId.kLastWeek,
    });
    await microtasksFinished();

    element.$.router.selectedPage = 'grouped';
    await microtasksFinished();

    element.$.router.selectedPage = 'history';
    await microtasksFinished();

    filterChips = element.shadowRoot.querySelector<HistoryFilterChipsElement>(
        '#historyFilterChips');
    assertTrue(!!filterChips);
    assertFalse(filterChips.userVisits);
    assertTrue(filterChips.actorVisits);
    assertDeepEquals(['id2', 'id3'], filterChips.clientIds);
    assertEquals(
        TimePeriodOptionId.kLastWeek, filterChips.selectedTimePeriodOptionId);
  });
});

suite('WebuiRefresh2026', function() {
  const WEBUI_REFRESH_ATTR = 'webui-refresh-2026';
  let element: HistoryAppElement;
  let browserProxy: TestHistoryBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
  });

  function createPage() {
    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  }

  test('Enabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: WEBUI_REFRESH_ATTR});
    await createPage();

    assertNotEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });

  // Android links colors.css whether or not the refresh is enabled.
  // <if expr="not is_android">
  test('Disabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: ''});
    await createPage();

    assertEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });
  // </if>
});

// <if expr="is_android">
suite('HistoryAppTestAndroid', function() {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      webuiRefresh2026: 'webui-refresh-2026',
    });

    BrowserProxyImpl.setInstance(new TestHistoryBrowserProxy());

    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
  });

  test('ColorsCssLinkedOnce', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: ''});
    // Count the page's insertions. ColorChangeUpdater can replace the link
    // with one that has a version parameter, so those are not counted.
    let added = 0;
    const count = (records: MutationRecord[]) => {
      for (const record of records) {
        for (const node of record.addedNodes) {
          if (node instanceof Element && node.matches(COLORS_CSS_SELECTOR) &&
              !new URL(node.getAttribute('href')!, location.href)
                   .searchParams.has('version')) {
            added++;
          }
        }
      }
    };
    const observer = new MutationObserver(count);
    observer.observe(document.body, {childList: true});
    document.body.appendChild(document.createElement('history-app'));
    await microtasksFinished();
    count(observer.takeRecords());
    observer.disconnect();

    assertEquals(1, added);
  });

  test('FontFamily', async () => {
    const app = document.createElement('history-app');
    document.body.appendChild(app);
    await microtasksFinished();
    assertTrue(getComputedStyle(app).fontFamily.includes('Google Sans Text'));
  });
});
// </if>
