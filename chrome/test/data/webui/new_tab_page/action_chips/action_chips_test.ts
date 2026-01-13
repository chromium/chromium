// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {ActionChipsHandlerRemote, ChipType, PageCallbackRouter} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import type {ActionChip, PageRemote, TabInfo} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import {ActionChipsApiProxyImpl, ActionChipsRetrievalState} from 'chrome://new-tab-page/lazy_load.js';
import type {ActionChipsElement} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {TabUpload} from 'chrome://resources/cr_components/composebox/common.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

suite('NewTabPageActionChipsTest', () => {
  let chips: ActionChipsElement;
  let handler: TestMock<ActionChipsHandlerRemote>;
  let pageRemote: PageRemote;
  let windowProxy: TestMock<WindowProxy>;

  interface InitializeChipsOptions {
    actionChips: ActionChip[];
    windowTimestampStart: number;
    windowTimestampEnd: number;
  }

  async function initializeChips(
      providedOptions: Partial<InitializeChipsOptions>): Promise<void> {
    const defaultOptions: InitializeChipsOptions = {
      actionChips: [
        {
          type: ChipType.kRecentTab,
          title: 'Example Tab',
          suggestion: 'Ask about this tab',
          tab: {
            tabId: 1,
            url: {url: 'https://example.com/test'},
            title: 'Example Tab',
            lastActiveTime: {internalValue: BigInt(12345)},
          },
        },
        {
          type: ChipType.kImage,
          title: 'Nano Banana',
          suggestion: 'Create an image of a nano banana',
          tab: null,
        },
        {
          type: ChipType.kDeepSearch,
          title: 'Deep Search',
          suggestion: 'Search for something deep',
          tab: null,
        },
      ],
      windowTimestampStart: Date.now().valueOf(),
      windowTimestampEnd: Date.now().valueOf() + 1,
    };
    const options = {...defaultOptions, ...providedOptions};
    handler.setResultMapperFor('startActionChipsRetrieval', () => {
      pageRemote.onActionChipsChanged(options.actionChips);
      pageRemote.$.flushForTesting();
    });

    // Timestamp recorded when the action chips element is constructed.
    windowProxy.setResultFor('now', options.windowTimestampStart);

    loadTimeData.overrideValues({
      addTabUploadDelayOnActionChipClick: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chips = document.createElement('ntp-action-chips');

    // Timestamp recorded after the action chips have been updated.
    windowProxy.setResultFor('now', options.windowTimestampEnd);

    document.body.append(chips);
    await handler.whenCalled('startActionChipsRetrieval');
    await microtasksFinished();
  }

  setup(() => {
    const callbackRouter = new PageCallbackRouter();
    handler = installMock(ActionChipsHandlerRemote, (mock) => {
      ActionChipsApiProxyImpl.setInstance({
        getHandler: () => mock,
        getCallbackRouter: () => callbackRouter,
      });
    });
    windowProxy = installMock(WindowProxy);
    pageRemote = callbackRouter.$.bindNewPipeAndPassRemote();
  });

  test('should render base chips', async () => {
    await initializeChips({});
    const actionChipsContainer =
        chips.shadowRoot.querySelector<HTMLHtmlElement>(
            '.action-chips-wrapper');
    assertTrue(!!actionChipsContainer);
  });

  test('clicking recent tab chip creates correct TabUpload file', async () => {
    const fakeTab: TabInfo = {
      tabId: 1,
      title: 'Test Title',
      url: {url: 'https://example.com/test'},
      lastActiveTime: {internalValue: BigInt(12345)},
    };
    await initializeChips({
      actionChips: [
        {
          type: ChipType.kRecentTab,
          title: 'Example Tab',
          suggestion: 'Ask about this tab',
          tab: fakeTab,
        },
      ],
    });

    const recentTabChip =
        chips.shadowRoot.querySelector<HTMLButtonElement>('#tab-context');
    assertTrue(!!recentTabChip);
    const whenActionChipClicked =
        eventToPromise('action-chip-click', document.body);
    recentTabChip.click();
    const event = await whenActionChipClicked;
    const expectedTab: TabUpload = {
      tabId: fakeTab.tabId,
      url: fakeTab.url,
      title: fakeTab.title,
      delayUpload: true,
      origin: TabUploadOrigin.ACTION_CHIP,
    };

    assertTrue(!!event.detail.contextFiles);
    assertEquals(1, event.detail.contextFiles.length);
    assertDeepEquals(expectedTab, event.detail.contextFiles[0]);
  });

  test('recent tab chip renders favicon', async () => {
    await initializeChips({
      actionChips: [{
        type: ChipType.kRecentTab,
        title: 'Example Tab',
        suggestion: 'Ask about this tab',
        tab: {
          url: {url: 'https://example.com'},
          tabId: 0,
          title: 'Example Tab',
          lastActiveTime: {internalValue: BigInt(0)},
        },
      }],
    });
    const recentTabChipIcon = chips.shadowRoot.querySelector<HTMLImageElement>(
        '.action-chip-recent-tab-favicon');
    assertTrue(!!recentTabChipIcon);
  });

  test('deep dive chip renders correct format', async () => {
    await initializeChips({
      actionChips: [{
        type: ChipType.kDeepDive,
        title: 'Example Tab',
        suggestion: 'Ask more about this site',
        tab: {
          url: {url: 'https://example.com'},
          tabId: 0,
          title: 'Example Tab',
          lastActiveTime: {internalValue: BigInt(0)},
        },
      }],
    });

    // Check correct classes are rendered
    const deepDiveChipIcon = chips.shadowRoot.querySelector<HTMLElement>(
        '.action-chip-icon-container.deep-dive');
    assertTrue(!!deepDiveChipIcon);

    // Check chip title is not rendered
    const deepDiveChipTitle =
        chips.shadowRoot.querySelector<HTMLImageElement>('.chip-title');
    assertEquals(null, deepDiveChipTitle);
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

      assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.ActionChips.Click', ChipType.kImage));
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

      assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
      assertEquals(
          1,
          metrics.count('NewTabPage.ActionChips.Click', ChipType.kDeepSearch));
    });

    test('tab context chip triggers chip click event', async () => {
      // Setup.
      const recentTabChip =
          chips.shadowRoot.querySelector<HTMLButtonElement>('#tab-context');
      assertTrue(!!recentTabChip);
      const whenActionChipClicked =
          eventToPromise('action-chip-click', document.body);
      recentTabChip.click();

      // Assert.
      await whenActionChipClicked;

      assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
      assertEquals(
          1,
          metrics.count('NewTabPage.ActionChips.Click', ChipType.kRecentTab));
    });

    test('deep dive chip triggers chip click event', async () => {
      // Setup.
      await initializeChips({
        actionChips: [{
          type: ChipType.kDeepDive,
          title: 'Example Tab',
          suggestion: 'Help me with this page',
          tab: {
            url: {url: 'https://example.com'},
            tabId: 0,
            title: 'Example Tab',
            lastActiveTime: {internalValue: BigInt(0)},
          },
        }],
      });
      const deepDiveChip =
          chips.shadowRoot.querySelector<HTMLButtonElement>('#deep-dive-0');
      assertTrue(!!deepDiveChip);

      const whenActionChipClicked =
          eventToPromise('action-chip-click', document.body);

      // Act.
      deepDiveChip.click();

      // Assert.
      await whenActionChipClicked;

      assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.ActionChips.Click', ChipType.kDeepDive));
    });
  });

  test('latency is recorded once for non-empty action chips', async () => {
    // Setup.
    const metrics: MetricsTracker = fakeMetricsPrivate();
    const expectedLatency = 1000;
    const mockTimestamp = Date.now().valueOf();

    // Act.
    await initializeChips({
      windowTimestampStart: mockTimestamp,
      windowTimestampEnd: mockTimestamp + expectedLatency,
    });

    // Assert.
    assertEquals(
        1, metrics.count('NewTabPage.ActionChips.WebUI.InitialLoadLatency'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ActionChips.WebUI.InitialLoadLatency',
            expectedLatency));
  });

  test('latency is not recorded for empty action chips', async () => {
    // Setup.
    const metrics: MetricsTracker = fakeMetricsPrivate();
    const expectedLatency = 1000;
    const mockTimestamp = Date.now().valueOf();

    // Act.
    await initializeChips({
      actionChips: [],
      windowTimestampStart: mockTimestamp,
      windowTimestampEnd: mockTimestamp + expectedLatency,
    });

    // Assert.
    assertEquals(
        0, metrics.count('NewTabPage.ActionChips.WebUI.InitialLoadLatency'));
  });

  suite('startActionChipsRetrieval', () => {
    test(
        'Handler is called on load and its completion fires an event',
        async () => {
          const events: ActionChipsRetrievalState[] = [];
          const eventCollector = (e: any) => {
            events.push(e.detail.state);
          };
          document.body.addEventListener(
              'action-chips-retrieval-state-changed', eventCollector);
          await initializeChips({});
          assertEquals(1, handler.getCallCount('startActionChipsRetrieval'));
          await microtasksFinished();
          document.body.removeEventListener(
              'action-chips-retrieval-state-changed', eventCollector);
          assertDeepEquals(
              [
                ActionChipsRetrievalState.REQUESTED,
                ActionChipsRetrievalState.UPDATED,
              ],
              events);
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
                             ?.textContent.trim(),
                  body:
                      spans
                          .find(
                              (e: Element) => e.classList.contains('chip-body'))
                          ?.textContent.trim(),
                };
              }));
        });

    test('should dismiss a chip when remove button is clicked', async () => {
      loadTimeData.overrideValues({
        ntpNextShowDismissalUIEnabled: true,
      });
      await initializeChips({});
      let allChips =
          chips.shadowRoot.querySelectorAll<HTMLButtonElement>('.action-chip');
      assertEquals(3, allChips.length);

      const removeButton = chips.shadowRoot.querySelector<HTMLButtonElement>(
          '.chip-remove-button');
      assertTrue(!!removeButton, 'Remove button should be present');

      removeButton.click();
      await microtasksFinished();

      allChips =
          chips.shadowRoot.querySelectorAll<HTMLButtonElement>('.action-chip');
      assertEquals(2, allChips.length);
    });
  });
});
