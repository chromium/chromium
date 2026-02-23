// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {BrowserServiceImpl} from 'chrome://history/history.js';
import type {HistoryFilterChipsElement} from 'chrome://history/history.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  test('RecordsActionWhenUserVisitsToggled', async () => {
    const shadowRoot = element.shadowRoot;
    assertTrue(!!shadowRoot);

    const userChip = shadowRoot.querySelector<HTMLElement>('#userVisitsChip');
    assertTrue(!!userChip);

    // Show user visits only.
    userChip.click();
    let action = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowUserOnlyEnabled', action);
    browserService.resetResolver('recordAction');

    // Show all visits by a second click on the user chip.
    userChip.click();
    action = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowAllEnabled', action);
  });

  test('RecordsActionWhenActorVisitsToggled', async () => {
    const shadowRoot = element.shadowRoot;
    assertTrue(!!shadowRoot);

    const actorChip = shadowRoot.querySelector<HTMLElement>('#actorVisitsChip');
    assertTrue(!!actorChip);

    // Show actor visits only.
    actorChip.click();
    let action = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowActorOnlyEnabled', action);
    browserService.resetResolver('recordAction');

    // Show all visits by a second click on the actor chip.
    actorChip.click();
    action = await browserService.whenCalled('recordAction');
    assertEquals('HistoryPage_ShowAllEnabled', action);
  });
});
