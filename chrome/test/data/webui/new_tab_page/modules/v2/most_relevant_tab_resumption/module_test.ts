// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DismissModuleElementEvent, DismissModuleInstanceEvent, MostRelevantTabResumptionModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {mostRelevantTabResumptionDescriptor, MostRelevantTabResumptionProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {PageHandlerRemote, ScoredURLUserAction} from 'chrome://new-tab-page/most_relevant_tab_resumption.mojom-webui.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {DecorationType, FormFactor, VisitSource} from 'chrome://new-tab-page/url_visit_types.mojom-webui.js';
import type {URLVisit} from 'chrome://new-tab-page/url_visit_types.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

function createSampleURLVisits(count: number): URLVisit[] {
  return new Array(count).fill(0).map(
      (_, i) => createSampleURLVisit({sessionName: i.toString()}));
}

function createSampleURLVisit(
    overrides?: Partial<URLVisit>,
    ): URLVisit {
  const url_visit: URLVisit = Object.assign(
      {
        decoration: {
          type: DecorationType.kVisitedXAgo,
          displayString: 'You visited 0 seconds ago',
        },
        formFactor: FormFactor.kDesktop,
        sessionName: 'Test Device',
        url: {url: 'https://www.foo.com'},
        urlKey: '',
        title: 'Test Tab Title',
        timestamp: Date.now(),
        trainingRequestId: 0,
        relativeTime: {microseconds: BigInt(0)},
        relativeTimeText: '0 seconds ago',
        source: VisitSource.kTab,
      },
      overrides);

  return url_visit;
}

suite('NewTabPageModulesMostRelevantTabResumptionModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let metrics: MetricsTracker;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      modulesRedesignedEnabled: true,
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => MostRelevantTabResumptionProxyImpl.setInstance(
            new MostRelevantTabResumptionProxyImpl(mock)));
    metrics = fakeMetricsPrivate();
  });

  async function initializeModule(urlVisits: URLVisit[]):
      Promise<MostRelevantTabResumptionModuleElement> {
    handler.setResultFor('getURLVisits', Promise.resolve({urlVisits}));
    const moduleElement = await mostRelevantTabResumptionDescriptor.initialize(
                              0) as MostRelevantTabResumptionModuleElement;
    document.body.append(moduleElement);

    await waitAfterNextRender(document.body);
    return moduleElement;
  }

  suite('Core', () => {
    test('No module created if no tab resumption data', async () => {
      // Arrange.
      const moduleElement = await initializeModule([]);

      // Assert.
      assertEquals(null, moduleElement);
    });

    test('Module instance created successfully', async () => {
      const moduleElement = await initializeModule(createSampleURLVisits(1));
      assertTrue(!!moduleElement);
    });

    test('Header element populated with correct data', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleURLVisits(1));

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);
      const actionMenu = $$(headerElement, 'cr-action-menu');
      assertTrue(!!actionMenu);

      const actionMenuItems =
          [...actionMenu.querySelectorAll('button.dropdown-item')];
      assertEquals(4, actionMenuItems.length);
      ['dismiss', 'disable', 'info', 'customize-module'].forEach(
          (action, index) => {
            assertEquals(
                action, actionMenuItems[index]!.getAttribute('data-action'));
          });
    });

    test('Header info button click opens info dialog', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleURLVisits(1));

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);
      const infoButton = $$<HTMLElement>(headerElement, '#info');
      assertTrue(!!infoButton);
      infoButton!.click();
      await microtasksFinished();

      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });

    test('Header dismiss button dispatches dismiss module event', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleURLVisits(1));

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);
      const dismissButton = $$<HTMLElement>(headerElement, '#dismiss');
      assertTrue(!!dismissButton);
      const waitForDismissEvent =
          eventToPromise('dismiss-module-instance', moduleElement);
      dismissButton!.click();
      await microtasksFinished();

      const dismissEvent: DismissModuleInstanceEvent =
          await waitForDismissEvent;
      assertEquals(`Tabs hidden`, dismissEvent.detail.message);

      // Act.
      const restoreCallback = dismissEvent.detail.restoreCallback!;
      restoreCallback();
      assertTrue(!!moduleElement);
    });

    test('Tab dismiss button dispatches dismiss tab event', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleURLVisits(1));

      // Assert.
      assertTrue(!!moduleElement);
      const waitForDismissEvent =
          eventToPromise('dismiss-module-element', moduleElement);
      const dismissButton = $$<HTMLElement>(moduleElement, 'cr-icon-button');
      assertTrue(!!dismissButton);
      dismissButton!.click();
      await microtasksFinished();

      const dismissEvent: DismissModuleElementEvent = await waitForDismissEvent;
      assertEquals(`Tabs hidden`, dismissEvent.detail.message);
      assertEquals(
          1, metrics.count(`NewTabPage.TabResumption.VisitDismissIndex`, 0));

      // Act.
      const restoreCallback = dismissEvent.detail.restoreCallback!;
      restoreCallback();
      assertTrue(!!moduleElement);
      assertEquals(
          1, metrics.count(`NewTabPage.TabResumption.VisitRestoreIndex`, 0));
    });

    test('Tab click fires usage event', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleURLVisits(1));

      // Assert.
      assertTrue(!!moduleElement);
      const urlVisitElement = $$<HTMLElement>(moduleElement, '.url-visit');
      assertTrue(!!urlVisitElement);
      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      urlVisitElement!.removeAttribute('href');
      urlVisitElement!.click();
      await microtasksFinished();
      assertEquals(1, metrics.count(`NewTabPage.TabResumption.ClickIndex`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.TabResumption.Visit.ClickSource`, VisitSource.kTab));
      assertEquals(
          ScoredURLUserAction.kSeen, handler.getArgs('recordAction')[0][0]);
      assertEquals(
          ScoredURLUserAction.kActivated,
          handler.getArgs('recordAction')[1][0]);

      await waitForUsageEvent;
    });

    test('See More click fires usage event', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleURLVisits(1));

      // Assert.
      assertTrue(!!moduleElement);
      const seeMoreButtonElement =
          ($$(moduleElement,
              '#seeMoreButtonContainer'))!.querySelector<HTMLElement>('a');
      assertTrue(!!seeMoreButtonElement);
      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      seeMoreButtonElement!.removeAttribute('href');
      seeMoreButtonElement!.click();
      assertEquals(1, metrics.count(`NewTabPage.TabResumption.SeeMoreClick`));
      await waitForUsageEvent;
    });
  });
});
