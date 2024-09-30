// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import type {AutoTabGroupsPageElement, AutoTabGroupsResultsElement, CrInputElement, TabOrganizationSession} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabOrganizationError, TabOrganizationState, TabSearchApiProxyImpl, TabSearchSyncBrowserProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createProfileData, createTab, createTabOrganizationSession} from './tab_search_test_data.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';
import {TestTabSearchSyncBrowserProxy} from './test_tab_search_sync_browser_proxy.js';

suite('AutoTabGroupsPageTest', () => {
  let autoTabGroupsPage: AutoTabGroupsPageElement;
  let autoTabGroupsResults: AutoTabGroupsResultsElement;
  let testApiProxy: TestTabSearchApiProxy;
  let testSyncProxy: TestTabSearchSyncBrowserProxy;

  function autoTabGroupsPageSetup() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testApiProxy = new TestTabSearchApiProxy();
    testApiProxy.setProfileData(createProfileData());
    const session = createTabOrganizationSession();
    testApiProxy.setSession(session);
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    testSyncProxy = new TestTabSearchSyncBrowserProxy();
    TabSearchSyncBrowserProxyImpl.setInstance(testSyncProxy);

    autoTabGroupsPage = document.createElement('auto-tab-groups-page');
    document.body.appendChild(autoTabGroupsPage);
    autoTabGroupsPage.setSessionForTesting(session);
    return microtasksFinished();
  }

  function autoTabGroupsResultsSetup() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testApiProxy = new TestTabSearchApiProxy();
    testApiProxy.setProfileData(createProfileData());
    const session = createTabOrganizationSession();
    testApiProxy.setSession(session);
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    testSyncProxy = new TestTabSearchSyncBrowserProxy();
    TabSearchSyncBrowserProxyImpl.setInstance(testSyncProxy);

    autoTabGroupsResults = document.createElement('auto-tab-groups-results');
    autoTabGroupsResults.multiTabOrganization = false;
    autoTabGroupsResults.session = session;

    document.body.appendChild(autoTabGroupsResults);
    return microtasksFinished();
  }

  function createMultiOrganizationSession(
      count: number, override: Partial<TabOrganizationSession> = {}) {
    const organizations: Object[] = [];
    for (let i = 0; i < count; i++) {
      organizations.push(
          {
            organizationId: i,
            name: stringToMojoString16('Organization ' + i),
            tabs: [
              createTab({
                title: 'Tab 1 Organization ' + i,
                url: {url: 'https://tab-1.com/'},
              }),
              createTab({
                title: 'Tab 2 Organization ' + i,
                url: {url: 'https://tab-2.com/'},
              }),
              createTab({
                title: 'Tab 3 Organization ' + i,
                url: {url: 'https://tab-3.com/'},
              }),
            ],
          },
      );
    }

    return Object.assign(
        {
          activeTabId: -1,
          sessionId: 1,
          state: TabOrganizationState.kSuccess,
          organizations: organizations,
          error: TabOrganizationError.kNone,
        },
        override);
  }

  test('Organize tabs starts request', async () => {
    await autoTabGroupsPageSetup();
    assertEquals(0, testApiProxy.getCallCount('requestTabOrganization'));
    const notStarted = autoTabGroupsPage.shadowRoot!.querySelector(
        'auto-tab-groups-not-started');
    assertTrue(!!notStarted);
    assertTrue(isVisible(notStarted));

    const actionButton = notStarted.shadowRoot!.querySelector('cr-button');
    assertTrue(!!actionButton);
    actionButton.click();

    assertEquals(1, testApiProxy.getCallCount('requestTabOrganization'));
  });

  test('Single organization input blurs on enter', async () => {
    await autoTabGroupsResultsSetup();
    const group =
        autoTabGroupsResults.shadowRoot!.querySelector('auto-tab-groups-group');
    assertTrue(!!group);
    const input = group.shadowRoot!.querySelector<CrInputElement>(
        '#singleOrganizationInput');
    assertTrue(!!input);
    assertTrue(input.hasAttribute('focused_'));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await input.updateComplete;
    assertFalse(input.hasAttribute('focused_'));
  });

  test('Multi organization input toggles on enter/edit', async () => {
    await autoTabGroupsResultsSetup();
    autoTabGroupsResults.multiTabOrganization = true;
    await microtasksFinished();

    function queryInput() {
      const group = autoTabGroupsResults.shadowRoot!.querySelector(
          'auto-tab-groups-group');
      assertTrue(!!group);
      return group.shadowRoot!.querySelector<HTMLElement>(
          '#multiOrganizationInput');
    }

    function queryEditButton() {
      const group = autoTabGroupsResults.shadowRoot!.querySelector(
          'auto-tab-groups-group');
      assertTrue(!!group);
      return group.shadowRoot!.querySelector<HTMLElement>('.icon-edit');
    }

    let input = queryInput();
    assertTrue(!!input);
    assertTrue(isVisible(input));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    assertFalse(!!queryInput());

    const editButton = queryEditButton();
    assertTrue(!!editButton);
    assertTrue(isVisible(editButton));

    editButton.click();
    await microtasksFinished();

    input = queryInput();
    assertTrue(!!input);
    assertTrue(isVisible(input));
    assertFalse(!!queryEditButton());
  });

  test('Rename propagates to api proxy', async () => {
    await autoTabGroupsPageSetup();

    const results =
        autoTabGroupsPage.shadowRoot!.querySelector('auto-tab-groups-results');
    assertTrue(!!results);
    results.dispatchEvent(new CustomEvent(
        'name-change', {detail: {organizationId: 1, name: 'new-name'}}));

    const [_sessionId, organizationId, name] =
        await testApiProxy.whenCalled('renameTabOrganization');
    assertEquals(1, organizationId);
    assertEquals('new-name', name);
  });

  test('Tab close removes from tab list', async () => {
    await autoTabGroupsPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createTabOrganizationSession({state: TabOrganizationState.kSuccess}));
    await microtasksFinished();

    const results =
        autoTabGroupsPage.shadowRoot!.querySelector('auto-tab-groups-results');
    assertTrue(!!results);
    const group = results.shadowRoot!.querySelector('auto-tab-groups-group');
    assertTrue(!!group);

    assertEquals(0, testApiProxy.getCallCount('removeTabFromOrganization'));

    const tabRows = group.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(3, tabRows.length);

    const cancelButton =
        tabRows[0]!.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!cancelButton);
    cancelButton.click();

    assertEquals(1, testApiProxy.getCallCount('removeTabFromOrganization'));
  });

  test('Arrow keys traverse focus in results list', async () => {
    await autoTabGroupsResultsSetup();

    const group =
        autoTabGroupsResults.shadowRoot!.querySelector('auto-tab-groups-group');
    assertTrue(!!group);
    const tabRows = group.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(3, tabRows.length);

    const closeButton0 =
        tabRows[0]!.shadowRoot!.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton0);
    const closeButton1 =
        tabRows[1]!.shadowRoot!.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton1);
    const closeButton2 =
        tabRows[2]!.shadowRoot!.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton2);

    closeButton0.focus();

    assertTrue(closeButton0.matches(':focus'));
    assertFalse(closeButton1.matches(':focus'));
    assertFalse(closeButton2.matches(':focus'));

    group.$.selector.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    await microtasksFinished();

    assertFalse(closeButton0.matches(':focus'));
    assertFalse(closeButton1.matches(':focus'));
    assertTrue(closeButton2.matches(':focus'));

    group.$.selector.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await microtasksFinished();

    assertTrue(closeButton0.matches(':focus'));
    assertFalse(closeButton1.matches(':focus'));
    assertFalse(closeButton2.matches(':focus'));
  });

  test('Arrow keys traverse focus in footer', async () => {
    await autoTabGroupsResultsSetup();

    const focusableElement0 = autoTabGroupsResults.$.learnMore;
    const focusableElement1 = autoTabGroupsResults.$.feedbackButtons.$.thumbsUp;
    const focusableElement2 =
        autoTabGroupsResults.$.feedbackButtons.$.thumbsDown;
    focusableElement0.focus();

    assertTrue(focusableElement0.matches(':focus'));
    assertFalse(focusableElement1.matches(':focus'));
    assertFalse(focusableElement2.matches(':focus'));

    const feedback =
        autoTabGroupsResults.shadowRoot!.querySelector('.feedback');
    assertTrue(!!feedback);
    feedback.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));

    assertFalse(focusableElement0.matches(':focus'));
    assertFalse(focusableElement1.matches(':focus'));
    assertTrue(focusableElement2.matches(':focus'));

    feedback.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));

    assertTrue(focusableElement0.matches(':focus'));
    assertFalse(focusableElement1.matches(':focus'));
    assertFalse(focusableElement2.matches(':focus'));
  });

  test('Single organization create group accepts organization', async () => {
    await autoTabGroupsPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createTabOrganizationSession({state: TabOrganizationState.kSuccess}));

    assertEquals(0, testApiProxy.getCallCount('acceptTabOrganization'));

    const results =
        autoTabGroupsPage.shadowRoot!.querySelector('auto-tab-groups-results');
    assertTrue(!!results);
    const group = results.shadowRoot!.querySelector('auto-tab-groups-group');
    assertTrue(!!group);
    const actions =
        group.shadowRoot!.querySelector('auto-tab-groups-results-actions');
    assertTrue(!!actions);
    const createGroupButton = actions.shadowRoot!.querySelector('cr-button');
    assertTrue(!!createGroupButton);
    createGroupButton.click();
    await microtasksFinished();

    assertEquals(1, testApiProxy.getCallCount('acceptTabOrganization'));
  });

  test(
      'Multi organization create groups accepts all organizations',
      async () => {
        loadTimeData.overrideValues({
          multiTabOrganizationEnabled: true,
        });

        await autoTabGroupsPageSetup();

        const organizationCount = 3;
        testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
            createMultiOrganizationSession(organizationCount));
        await microtasksFinished();

        assertEquals(0, testApiProxy.getCallCount('acceptTabOrganization'));

        const results = autoTabGroupsPage.shadowRoot!.querySelector(
            'auto-tab-groups-results');
        assertTrue(!!results);
        const actions = results.shadowRoot!.querySelector(
            'auto-tab-groups-results-actions');
        assertTrue(!!actions);
        const createGroupsButton =
            actions.shadowRoot!.querySelector<HTMLElement>('#createButton');
        assertTrue(!!createGroupsButton);
        createGroupsButton.click();
        await microtasksFinished();

        assertEquals(
            organizationCount,
            testApiProxy.getCallCount('acceptTabOrganization'));
      });

  test('Group cancel button rejects organization', async () => {
    loadTimeData.overrideValues({
      multiTabOrganizationEnabled: true,
    });

    await autoTabGroupsPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createMultiOrganizationSession(
            3, {state: TabOrganizationState.kSuccess}));
    await microtasksFinished();

    assertEquals(0, testApiProxy.getCallCount('rejectTabOrganization'));

    const results =
        autoTabGroupsPage.shadowRoot!.querySelector('auto-tab-groups-results');
    assertTrue(!!results);
    const group = results.shadowRoot!.querySelector('auto-tab-groups-group');
    assertTrue(!!group);
    const cancelButton =
        group.shadowRoot!.querySelector<HTMLElement>('#rejectButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await microtasksFinished();

    assertEquals(1, testApiProxy.getCallCount('rejectTabOrganization'));
  });

  test('Clear rejects session', async () => {
    loadTimeData.overrideValues({
      multiTabOrganizationEnabled: true,
    });

    await autoTabGroupsPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createTabOrganizationSession({state: TabOrganizationState.kSuccess}));
    await microtasksFinished();

    assertEquals(0, testApiProxy.getCallCount('rejectSession'));

    const results =
        autoTabGroupsPage.shadowRoot!.querySelector('auto-tab-groups-results');
    assertTrue(!!results);
    const actions =
        results.shadowRoot!.querySelector('auto-tab-groups-results-actions');
    assertTrue(!!actions);
    const clearButton =
        actions.shadowRoot!.querySelector<HTMLElement>('#clearButton');
    assertTrue(!!clearButton);
    clearButton.click();
    await microtasksFinished();

    assertEquals(1, testApiProxy.getCallCount('rejectSession'));
  });

  test('Tip action activates on Enter', async () => {
    loadTimeData.overrideValues({
      showTabOrganizationFRE: true,
    });

    await autoTabGroupsPageSetup();

    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        createTabOrganizationSession({
          state: TabOrganizationState.kFailure,
          error: TabOrganizationError.kGeneric,
        }));

    assertEquals(0, testApiProxy.getCallCount('startTabGroupTutorial'));

    const failure =
        autoTabGroupsPage.shadowRoot!.querySelector('auto-tab-groups-failure');
    assertTrue(!!failure);
    const links = failure.shadowRoot!.querySelectorAll<HTMLElement>(
        '.auto-tab-groups-link');
    assertEquals(1, links.length);
    const tipAction = links[0]!;
    tipAction.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    assertEquals(1, testApiProxy.getCallCount('startTabGroupTutorial'));
  });

  test('Active tab missing from organization shows error', async () => {
    const errorString = 'error';
    const successString = 'success';
    loadTimeData.overrideValues({
      successMissingActiveTabTitle: errorString,
      successTitleSingle: successString,
    });
    await autoTabGroupsPageSetup();
    const session = createTabOrganizationSession({
      state: TabOrganizationState.kSuccess,
      activeTabId: 4,
      organizations: [{
        organizationId: 1,
        name: stringToMojoString16('foo'),
        firstNewTabIndex: 0,
        tabs: [
          createTab(
              {title: 'Tab 1', url: {url: 'https://tab-1.com/'}, tabId: 1}),
          createTab(
              {title: 'Tab 2', url: {url: 'https://tab-2.com/'}, tabId: 2}),
          createTab(
              {title: 'Tab 3', url: {url: 'https://tab-3.com/'}, tabId: 3}),
        ],
      }],
    });
    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        session);
    await microtasksFinished();

    const header = autoTabGroupsPage.shadowRoot!.querySelector('#header');
    assertTrue(!!header);
    assertEquals(errorString, header.textContent!.trim());
  });

  test('Active tab present in organization does not show error', async () => {
    const errorString = 'error';
    const successString = 'success';
    loadTimeData.overrideValues({
      successMissingActiveTabTitle: errorString,
      successTitleSingle: successString,
    });
    await autoTabGroupsPageSetup();
    const session = createTabOrganizationSession({
      state: TabOrganizationState.kSuccess,
      activeTabId: 2,
      organizations: [{
        organizationId: 1,
        name: stringToMojoString16('foo'),
        firstNewTabIndex: 0,
        tabs: [
          createTab(
              {title: 'Tab 1', url: {url: 'https://tab-1.com/'}, tabId: 1}),
          createTab(
              {title: 'Tab 2', url: {url: 'https://tab-2.com/'}, tabId: 2}),
          createTab(
              {title: 'Tab 3', url: {url: 'https://tab-3.com/'}, tabId: 3}),
        ],
      }],
    });
    testApiProxy.getCallbackRouterRemote().tabOrganizationSessionUpdated(
        session);
    await microtasksFinished();

    const header = autoTabGroupsPage.shadowRoot!.querySelector('#header');
    assertTrue(!!header);
    assertEquals(successString, header.textContent!.trim());
  });
});
