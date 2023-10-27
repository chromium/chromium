// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SyncInfo, Tab, TabOrganizationError, TabOrganizationPageElement, TabOrganizationResultsElement, TabOrganizationSession, TabOrganizationState, TabSearchApiProxyImpl, TabSearchSyncBrowserProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';
import {TestTabSearchSyncBrowserProxy} from './test_tab_search_sync_browser_proxy.js';

suite('TabOrganizationPageTest', () => {
  let tabOrganizationPage: TabOrganizationPageElement;
  let tabOrganizationResults: TabOrganizationResultsElement;
  let testApiProxy: TestTabSearchApiProxy;
  let testSyncProxy: TestTabSearchSyncBrowserProxy;

  async function tabOrganizationPageSetup(syncInfo: SyncInfo = {
    syncing: true,
    syncingHistory: true,
    paused: false,
  }) {
    testApiProxy = new TestTabSearchApiProxy();
    testApiProxy.setSession(createSession());
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    testSyncProxy = new TestTabSearchSyncBrowserProxy();
    testSyncProxy.syncInfo = syncInfo;
    TabSearchSyncBrowserProxyImpl.setInstance(testSyncProxy);

    tabOrganizationPage = document.createElement('tab-organization-page');

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabOrganizationPage);
    await flushTasks();
  }

  async function tabOrganizationResultsSetup() {
    testApiProxy = new TestTabSearchApiProxy();
    const session = createSession();
    testApiProxy.setSession(session);
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    testSyncProxy = new TestTabSearchSyncBrowserProxy();
    TabSearchSyncBrowserProxyImpl.setInstance(testSyncProxy);

    tabOrganizationResults = document.createElement('tab-organization-results');
    tabOrganizationResults.name = session.organizations[0]!.name;
    tabOrganizationResults.tabs = session.organizations[0]!.tabs;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabOrganizationResults);
    await flushTasks();
  }

  function createTab(override: Partial<Tab> = {}): Tab {
    return Object.assign(
        {
          active: false,
          alertStates: [],
          index: -1,
          tabId: -1,
          groupId: -1,
          pinned: false,
          title: '',
          url: {url: 'about:blank'},
          isDefaultFavicon: false,
          showIcon: false,
          lastActiveTimeTicks: -1,
          lastActiveElapsedText: '',
        },
        override);
  }

  function createSession(override: Partial<TabOrganizationSession> = {}):
      TabOrganizationSession {
    return Object.assign(
        {
          sessionId: 1,
          state: TabOrganizationState.kNotStarted,
          organizations: [{
            organizationId: 1,
            name: 'foo',
            tabs: [
              createTab({title: 'Tab 1', url: {url: 'https://tab-1.com/'}}),
              createTab({title: 'Tab 2', url: {url: 'https://tab-2.com/'}}),
              createTab({title: 'Tab 3', url: {url: 'https://tab-3.com/'}}),
            ],
          }],
          error: TabOrganizationError.kNone,
        },
        override);
  }

  test('Organize tabs starts request', async () => {
    await tabOrganizationPageSetup();
    assertEquals(0, testApiProxy.getCallCount('requestTabOrganization'));
    const notStarted = tabOrganizationPage.shadowRoot!.querySelector(
        'tab-organization-not-started');
    assertTrue(!!notStarted);
    assertTrue(isVisible(notStarted));

    const organizeTabsButton =
        notStarted.shadowRoot!.querySelector('cr-button');
    assertTrue(!!organizeTabsButton);
    organizeTabsButton.click();

    assertEquals(1, testApiProxy.getCallCount('requestTabOrganization'));
  });

  test('Input blurs on enter', async () => {
    await tabOrganizationResultsSetup();
    const input = tabOrganizationResults.$.input;
    assertFalse(input.hasAttribute('focused_'));

    input.focus();
    assertTrue(input.hasAttribute('focused_'));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertFalse(input.hasAttribute('focused_'));
  });

  test('Tab close removes from tab list', async () => {
    await tabOrganizationResultsSetup();

    const tabRows =
        tabOrganizationResults.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(3, tabRows.length);

    const cancelButton =
        tabRows[0]!.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await flushTasks();

    const tabRowsAfterCancel =
        tabOrganizationResults.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRowsAfterCancel);
    assertEquals(2, tabRowsAfterCancel.length);
  });

  test('Create group accepts organization', async () => {
    await tabOrganizationPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createSession({state: TabOrganizationState.kSuccess}));

    assertEquals(0, testApiProxy.getCallCount('acceptTabOrganization'));

    const results = tabOrganizationPage.shadowRoot!.querySelector(
        'tab-organization-results');
    assertTrue(!!results);
    const createGroupButton = results.shadowRoot!.querySelector('cr-button');
    assertTrue(!!createGroupButton);
    createGroupButton.click();
    await flushTasks();

    assertEquals(1, testApiProxy.getCallCount('acceptTabOrganization'));
  });

  test('Sync required for organization', async () => {
    const syncInfo: SyncInfo = {
      syncing: false,
      syncingHistory: false,
      paused: false,
    };
    await tabOrganizationPageSetup(syncInfo);

    const notStarted = tabOrganizationPage.shadowRoot!.querySelector(
        'tab-organization-not-started');
    assertTrue(!!notStarted);
    assertTrue(isVisible(notStarted));

    const actionButton = notStarted.shadowRoot!.querySelector('cr-button');
    assertTrue(!!actionButton);
    actionButton.click();

    // The action button should not request tab organization if the user is in
    // an invalid sync state.
    assertEquals(0, testApiProxy.getCallCount('requestTabOrganization'));
  });

  test('Updates with sync changes', async () => {
    await tabOrganizationPageSetup();

    const notStarted = tabOrganizationPage.shadowRoot!.querySelector(
        'tab-organization-not-started');
    assertTrue(!!notStarted);
    assertTrue(isVisible(notStarted));

    const accountRowSynced =
        notStarted.shadowRoot!.querySelector('.account-row');
    assertFalse(!!accountRowSynced);

    testSyncProxy.syncInfo = {
      syncing: false,
      syncingHistory: false,
      paused: false,
    };
    webUIListenerCallback('sync-info-changed');
    await testSyncProxy.whenCalled('getSyncInfo');

    const accountRowUnsynced =
        notStarted.shadowRoot!.querySelector('.account-row');
    assertTrue(!!accountRowUnsynced);
  });

  test('Tip action starts tutorial', async () => {
    loadTimeData.overrideValues({
      showTabOrganizationFRE: true,
    });

    await tabOrganizationPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createSession({
          state: TabOrganizationState.kFailure,
          error: TabOrganizationError.kGeneric,
        }));

    assertEquals(0, testApiProxy.getCallCount('startTabGroupTutorial'));

    const tipAction =
        tabOrganizationPage.shadowRoot!.querySelector<HTMLElement>('.link');
    assertTrue(!!tipAction);
    tipAction.click();
    await flushTasks();

    assertEquals(1, testApiProxy.getCallCount('startTabGroupTutorial'));
  });
});
