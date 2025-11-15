// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {ActionChipsHandlerRemote, ChipType} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import type {ActionChip, TabInfo} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import {ActionChipsApiProxyImpl, ActionChipsType} from 'chrome://new-tab-page/lazy_load.js';
import type {ActionChipsElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

suite('NewTabPageActionChipsTest', () => {
  let chips: ActionChipsElement;
  let handler: TestMock<ActionChipsHandlerRemote>;

  interface InitializeChipsOptions {
    tab: TabInfo|null;
    actionChips: ActionChip[];
  }

  async function initializeChips(
      providedOptions: Partial<InitializeChipsOptions>): Promise<void> {
    const defaultOptions: InitializeChipsOptions = {
      tab: null,
      actionChips: [
        {
          type: ChipType.kRecentTab,
          title: 'Example Tab',
          suggestion: 'Ask about this tab',
        },
        {
          type: ChipType.kImage,
          title: 'Nano Banana',
          suggestion: 'Create an image of a nano banana',
        },
        {
          type: ChipType.kDeepSearch,
          title: 'Deep Search',
          suggestion: 'Search for something deep',
        },
      ],
    };
    const options = {...defaultOptions, ...providedOptions};
    handler.setResultFor(
        'getMostRecentTab', Promise.resolve({tab: options.tab}));
    handler.setResultFor(
        'getActionChips', Promise.resolve({actionChips: options.actionChips}));

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chips = document.createElement('ntp-action-chips');
    document.body.append(chips);
    await Promise.all([
      handler.whenCalled('getActionChips'),
      handler.whenCalled('getMostRecentTab'),
    ]);
  }

  setup(() => {
    handler = installMock(
        ActionChipsHandlerRemote,
        mock => ActionChipsApiProxyImpl.setInstance({getHandler: () => mock}));
  });

  test('should render base chips', async () => {
    await initializeChips({});
    const actionChipsContainer =
        chips.shadowRoot.querySelector<HTMLHtmlElement>(
            '.action-chips-wrapper');
    assertTrue(!!actionChipsContainer);
  });

  test('recent tab chip triggers chip click event', async () => {
    // Setup.
    await initializeChips({});
    const recentTabChip =
        chips.shadowRoot.querySelector<HTMLButtonElement>('#tab-context');
    assertTrue(!!recentTabChip);
    const whenActionChipClicked =
        eventToPromise('action-chip-click', document.body);
    recentTabChip.click();

    // Assert.
    await whenActionChipClicked;
  });

  suite('metrics collection', () => {
    let metrics: MetricsTracker;
    setup(async () => {
      metrics = fakeMetricsPrivate();
      await initializeChips({});
    });
    test('nano banana chip triggers chip click event', async () => {
      // Setup.
      const nanoBananaChip =
          chips.shadowRoot.querySelector<HTMLButtonElement>('#nano-banana');
      assertTrue(!!nanoBananaChip);
      const whenActionChipClicked =
          eventToPromise('action-chip-click', document.body);
      nanoBananaChip.click();

      // Assert.
      await whenActionChipClicked;

      assertEquals(2, metrics.count('NewTabPage.ActionChips.Shown'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.ActionChips.Shown', ActionChipsType.CREATE_IMAGE));

      assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.ActionChips.Click', ActionChipsType.CREATE_IMAGE));
    });

    test('deep search chip triggers chip click event', async () => {
      // Setup.
      const deepSearchChip =
          chips.shadowRoot.querySelector<HTMLButtonElement>('#deep-search');
      assertTrue(!!deepSearchChip);
      const whenActionChipClicked =
          eventToPromise('action-chip-click', document.body);
      deepSearchChip.click();

      // Assert.
      await whenActionChipClicked;

      assertEquals(2, metrics.count('NewTabPage.ActionChips.Shown'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.ActionChips.Shown', ActionChipsType.DEEP_SEARCH));

      assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.ActionChips.Click', ActionChipsType.DEEP_SEARCH));
    });
  });

  // Suite for "Most Recent Tab" functionality
  suite('MostRecentTab', () => {
    test('Handler is called on load', async () => {
      await initializeChips({});

      assertTrue(!!chips);
      assertEquals(1, handler.getCallCount('getMostRecentTab'));
    });

    test('No tab info when no recent tab is returned', async () => {
      await initializeChips({});

      assertEquals(1, handler.getCallCount('getMostRecentTab'));
      assertEquals(null, chips.mostRecentTab);
    });

  test('Most recent tab info is found on return', async () => {
    const fakeTab: TabInfo = {
      tabId: 1,
      title: 'Test Title',
      url: {url: 'https://example.com/test'},
      lastActiveTime: {internalValue: BigInt(12345)},
    };
    await initializeChips({tab: fakeTab});

      assertEquals(1, handler.getCallCount('getMostRecentTab'));
      assertDeepEquals(fakeTab, chips.mostRecentTab);
    });
  });

  suite('getActionChips', () => {
    test('Handler is called on load', async () => {
      await initializeChips({});
      assertEquals(1, handler.getCallCount('getActionChips'));
    });

    test(
        'The number of chips is equal to the number of items in the response',
        async () => {
          await initializeChips({});
          assertTrue(!!chips);
          const allChips = Array.from<HTMLButtonElement>(
              chips.shadowRoot.querySelectorAll<HTMLButtonElement>('button'));
          assertEquals(3, allChips.length);
          assertTrue(allChips.every((e: HTMLButtonElement) => !!e));

          assertDeepEquals(
              [
                {
                  title: 'Example Tab',
                  body: 'Ask about this tab',
                },
                {
                  title: 'Nano Banana',
                  body: 'Create an image of a nano banana',
                },
                {
                  title: 'Deep Search',
                  body: 'Search for something deep',
                },
              ],
              allChips.map((chip: HTMLButtonElement) => {
                const spans =
                    Array.from<Element>(chip.querySelectorAll<Element>('span'));
                return {
                  title: spans
                             .find(
                                 (e: Element) =>
                                     e.classList.contains('chip-title'))
                             ?.textContent,
                  body:
                      spans
                          .find(
                              (e: Element) => e.classList.contains('chip-body'))
                          ?.textContent,
                };
              }));
        });
  });
});
