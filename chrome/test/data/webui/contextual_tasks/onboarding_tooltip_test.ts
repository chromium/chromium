// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';
import 'chrome://contextual-tasks/onboarding_tooltip.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ContextualTasksOnboardingTooltipElement} from 'chrome://contextual-tasks/onboarding_tooltip.js';
import {PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createCtComposeboxApp} from './contextual_tasks_test_utils.js';
import type {CtComposeboxAppParts} from './contextual_tasks_test_utils.js';
import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('OnboardingTooltipTest', () => {
  let tooltipElement: ContextualTasksOnboardingTooltipElement;
  let container: HTMLDivElement;
  let target: HTMLDivElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      composeboxShowOnboardingTooltipSessionImpressionCap: 3,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipImpressionDelay: 0,
      showOnboardingTooltip: true,
      tabFaviconChipsToCoinsEnabled: false,
    });

    // Create a positioned container.
    container = document.createElement('div');
    container.style.position = 'relative';
    container.style.width = '400px';
    container.style.height = '400px';
    document.body.appendChild(container);

    // Create a target element inside the container.
    target = document.createElement('div');
    target.style.position = 'absolute';
    target.style.bottom = '50px';
    target.style.left = '100px';
    target.style.width = '50px';
    target.style.height = '20px';
    container.appendChild(target);

    tooltipElement = document.createElement('contextual-tasks-onboarding-tooltip');
    container.appendChild(tooltipElement);
    await microtasksFinished();
  });

  test('shows and positions correctly', async () => {

    tooltipElement.updateTooltipVisibility(true, target, container);
    await microtasksFinished();

    assertTrue(tooltipElement.shouldShow);

    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertEquals('auto', crTooltip.style.bottom);
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');
  });

  test('hides tooltip when auto tab chip is removed', async () => {

    // Show tooltip.
    tooltipElement.updateTooltipVisibility(true, target, container);
    await microtasksFinished();
    assertTrue(tooltipElement.shouldShow);

    // Remove auto tab chip.
    tooltipElement.updateTooltipVisibility(false, target, container);
    await microtasksFinished();

    assertTrue(!tooltipElement.shouldShow);
  });



  test('positions correctly and resets bottom', async () => {
    tooltipElement.target = target;
    await microtasksFinished();

    // Manually set a bottom style to simulate the buggy state
    // (where CrTooltip fit-to-visible-bounds might have set bottom).
    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    crTooltip.style.bottom = '10px';

    // Call updatePosition which should reset bottom and set top.
    tooltipElement.updatePosition();
    await microtasksFinished();

    // Verify bottom is reset to auto.
    assertEquals('auto', crTooltip.style.bottom);

    // Verify top is set (should be positive/negative depending on layout,
    // but not empty and not 'auto').
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');
  });
});

[true, false].forEach(useFork => {
  suite(
      `ContextualTasksOnboardingTooltipForkTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        const AUTO_TOKEN = '0000000000000000AAAAAAAAAAAAAA01';

        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
        let parts: CtComposeboxAppParts;

        function autoTabInfo() {
          return {
            tabId: 1,
            title: 'Auto tab',
            url: 'https://auto.example.com',
            lastActive: {internalValue: BigInt(100)},
            showInCurrentTabChip: true,
            showInPreviousTabChip: false,
          };
        }

        setup(() => {
          if (!window.chrome) {
            Object.assign(window, {chrome: {}});
          }
          // Unconditionally replace any pre-existing stub; the composebox
          // metric paths call all three methods.
          Object.assign(window.chrome, {
            histograms: {
              recordEnumerationValue: () => {},
              recordUserAction: () => {},
              recordBoolean: () => {},
            },
          });
          document.body.innerHTML = window.trustedTypes!.emptyHTML;

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            contextManagementInComposeboxEnabled: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
            webUIOmniboxAskGAboutThisPageEnabled: false,
            forcedEmbeddedPageHost: '',
            tabFaviconChipsToCoinsEnabled: false,
            showOnboardingTooltip: true,
            isOnboardingTooltipDismissCountBelowCap: true,
            composeboxShowOnboardingTooltipSessionImpressionCap: 10,
            composeboxShowOnboardingTooltipImpressionDelay: 0,
          });

          // SlimWebview (Android) only allows https/http/about:blank in
          // navigate(); the chrome://webui-test fixture url would assert.
          const testProxy = new TestContextualTasksBrowserProxy('about:blank');
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'addTabContext', Promise.resolve(AUTO_TOKEN));
          // This file also runs on Android, where MockInputState is not built
          // and the searchbox getSmartTabSharingActive is absent from mojom.
          mockSearchboxPageHandler.setResultFor(
              'getInputState', Promise.resolve({
                state: {
                  allowedModels: [],
                  allowedTools: [],
                  allowedInputTypes: [],
                  activeModel: 0,
                  activeTool: 0,
                  disabledModels: [],
                  disabledTools: [],
                  disabledInputTypes: [],
                  toolConfigs: [],
                },
              }));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouterRemote =
              searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));
        });

        async function mountWithAutoTab():
            Promise<ComposeboxFileCarouselElement> {
          parts = await createCtComposeboxApp(useFork);
          searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
              autoTabInfo(), null);
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await mockSearchboxPageHandler.whenCalled('addTabContext');
          await microtasksFinished();
          await parts.innerComposebox.updateComplete;
          const carousel =
              parts.innerComposebox.shadowRoot
                  .querySelector<ComposeboxFileCarouselElement>('#carousel');
          assertTrue(!!carousel);
          await carousel.updateComplete;
          return carousel;
        }

        function getTooltip(): ContextualTasksOnboardingTooltipElement {
          const tooltip =
              parts.app.shadowRoot
                  .querySelector<ContextualTasksOnboardingTooltipElement>(
                      '#onboardingTooltip');
          assertTrue(!!tooltip);
          return tooltip;
        }

        test('anchors the tooltip on the real automatic tab chip', async () => {
          const carousel = await mountWithAutoTab();
          const {app, innerComposebox} = parts;
          assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

          app.updateTooltipVisibilityForTesting();
          await microtasksFinished();

          const tooltip = getTooltip();
          assertTrue(tooltip.shouldShow);
          const chip = innerComposebox.getAutomaticActiveTabChipElement();
          assertTrue(!!chip);
          assertEquals(chip, tooltip.target);
          assertEquals('CR-COMPOSEBOX-FILE-THUMBNAIL', chip.tagName);
          assertTrue(carousel.shadowRoot.contains(chip));
        });

        test('hides the tooltip when the automatic tab goes away', async () => {
          await mountWithAutoTab();
          const {app, innerComposebox} = parts;
          app.updateTooltipVisibilityForTesting();
          await microtasksFinished();
          const tooltip = getTooltip();
          assertTrue(tooltip.shouldShow);

          searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
              null, null);
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();
          await innerComposebox.updateComplete;
          await microtasksFinished();

          assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());
          app.updateTooltipVisibilityForTesting();
          await microtasksFinished();
          assertFalse(tooltip.shouldShow);
        });
      });
});
