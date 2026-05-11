// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {BrowserServiceImpl} from 'chrome://history/history.js';
import type {HistoryFilterChipsElement} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';

suite('HistoryFilterChipsTest', function() {
  let element: HistoryFilterChipsElement;
  let browserService: TestBrowserService;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes?.emptyHTML || '';

    browserService = new TestBrowserService();
    BrowserServiceImpl.setInstance(browserService);

    element = document.createElement('history-filter-chips');
    document.body.appendChild(element);

    await microtasksFinished();
    browserService.resetResolver('recordAction');
  });

  test('InitialState', () => {
    const userChip =
        element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
    const actorChip =
        element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
    assertTrue(!!userChip);
    assertTrue(!!actorChip);

    assertFalse(userChip.hasAttribute('selected'));
    assertFalse(actorChip.hasAttribute('selected'));
  });

  test('ToggleUserChip', async () => {
    const userChip =
        element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
    assertTrue(!!userChip);
    const userIcon = userChip.querySelector('cr-icon');
    assertTrue(!!userIcon);

    let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
    element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                 eventDetail = e.detail;
                                               }) as EventListener);

    // Show user visits only.
    userChip.click();
    await microtasksFinished();
    const action1 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowUserOnlyEnabled', action1);
    browserService.resetResolver('recordAction');

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertFalse(eventDetail.actorVisits);
    assertTrue(userChip.hasAttribute('selected'));
    assertEquals('cr:check', userIcon.icon);

    // Show all visits by a second click on the user chip.
    userChip.click();
    await microtasksFinished();
    const action2 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowAllEnabled', action2);

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertTrue(eventDetail.actorVisits);
    assertFalse(userChip.hasAttribute('selected'));
    assertEquals('cr:person', userIcon.icon);
  });

  test('ToggleActorChip', async () => {
    const actorChip =
        element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
    assertTrue(!!actorChip);
    const actorIcon = actorChip.querySelector('cr-icon');
    assertTrue(!!actorIcon);

    let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
    element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                 eventDetail = e.detail;
                                               }) as EventListener);

    // Show actor visits only.
    actorChip.click();
    const action1 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowActorOnlyEnabled', action1);
    await microtasksFinished();
    browserService.resetResolver('recordAction');

    assertTrue(!!eventDetail);
    assertFalse(eventDetail.userVisits);
    assertTrue(eventDetail.actorVisits);
    assertTrue(actorChip.hasAttribute('selected'));
    assertEquals('cr:check', actorIcon.icon);

    // Show all visits by a second click on the actor chip.
    actorChip.click();
    const action2 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowAllEnabled', action2);
    await microtasksFinished();

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertTrue(eventDetail.actorVisits);
    assertFalse(actorChip.hasAttribute('selected'));
    // <if expr="_google_chrome">
    assertEquals(
        'history-internal:screensaver-auto', actorIcon.icon);
    // </if>
    // <if expr="not _google_chrome">
    assertEquals('', actorIcon.icon);
    // </if>
  });

  test('ToggleBothChips', async () => {
    const userChip =
        element.shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
    const actorChip =
        element.shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
    assertTrue(!!userChip && !!actorChip);

    let eventDetail: {userVisits: boolean, actorVisits: boolean}|undefined;
    element.addEventListener('filter-changed', ((e: CustomEvent) => {
                                                 eventDetail = e.detail;
                                               }) as EventListener);

    // Start at 'User Only' by clicking the user chip.
    userChip.click();
    const action1 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowUserOnlyEnabled', action1);
    await microtasksFinished();
    browserService.resetResolver('recordAction');

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertFalse(eventDetail.actorVisits);
    assertTrue(userChip.hasAttribute('selected'));
    assertFalse(actorChip.hasAttribute('selected'));

    // Clicking Actor Chip while 'User Only' is active should
    // switch directly to 'Actor Only'.
    actorChip.click();
    const action2 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowActorOnlyEnabled', action2);
    await microtasksFinished();
    browserService.resetResolver('recordAction');

    assertTrue(!!eventDetail);
    assertFalse(eventDetail.userVisits);
    assertTrue(eventDetail.actorVisits);
    assertFalse(userChip.hasAttribute('selected'));
    assertTrue(actorChip.hasAttribute('selected'));

    // Clicking User Chip while 'Actor Only' is active should
    // switch back to 'User Only'.
    userChip.click();
    const action3 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowUserOnlyEnabled', action3);
    await microtasksFinished();
    browserService.resetResolver('recordAction');

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertFalse(eventDetail.actorVisits);
    assertTrue(userChip.hasAttribute('selected'));
    assertFalse(actorChip.hasAttribute('selected'));

    // Clicking User Chip again while 'User Only' is active
    // should return to 'Show All'.
    userChip.click();
    const action4 = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowAllEnabled', action4);
    await microtasksFinished();

    assertTrue(!!eventDetail);
    assertTrue(eventDetail.userVisits);
    assertTrue(eventDetail.actorVisits);
    assertFalse(userChip.hasAttribute('selected'));
    assertFalse(actorChip.hasAttribute('selected'));
  });
});
