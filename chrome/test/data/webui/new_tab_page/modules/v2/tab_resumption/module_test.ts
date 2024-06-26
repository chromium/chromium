// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Tab} from 'chrome://new-tab-page/history_types.mojom-webui.js';
import {DeviceType} from 'chrome://new-tab-page/history_types.mojom-webui.js';
import type {DismissModuleInstanceEvent, TabResumptionModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {tabResumptionDescriptor, TabResumptionProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/tab_resumption.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

function createSampleTabs(count: number): Tab[] {
  return new Array(count).fill(0).map(
      (_, i) => createSampleTab({sessionName: i.toString()}));
}

function createSampleTab(
    overrides?: Partial<Tab>,
    ): Tab {
  const tab: Tab = Object.assign(
      {
        decorator: null,
        deviceType: DeviceType.kDesktop,
        sessionName: 'Test Device',
        url: {url: 'https://www.foo.com'},
        urlKey: '',
        title: 'Test Tab Title',
        timestamp: Date.now(),
        trainingRequestId: 0,
        relativeTime: {microseconds: BigInt(0)},
        relativeTimeText: '0 seconds ago',
      },
      overrides);

  return tab;
}

suite('NewTabPageModulesTabResumptionModuleTest', () => {
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
        mock => TabResumptionProxyImpl.setInstance(
            new TabResumptionProxyImpl(mock)));
    metrics = fakeMetricsPrivate();
  });

  async function initializeModule(tabs: Tab[]):
      Promise<TabResumptionModuleElement> {
    handler.setResultFor('getTabs', Promise.resolve({tabs}));
    const moduleElement = await tabResumptionDescriptor.initialize(0) as
        TabResumptionModuleElement;
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
      const moduleElement = await initializeModule(createSampleTabs(1));
      assertTrue(!!moduleElement);
    });

    test('Header element populated with correct data', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleTabs(1));

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
      const moduleElement = await initializeModule(createSampleTabs(1));

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);
      headerElement!.dispatchEvent(new Event('info-button-click'));

      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });

    test('Header dismiss button dispatches dismiss module event', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleTabs(1));

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);
      const waitForDismissEvent =
          eventToPromise('dismiss-module-instance', moduleElement);
      headerElement!.dispatchEvent(new Event('dismiss-button-click'));

      const dismissEvent: DismissModuleInstanceEvent =
          await waitForDismissEvent;
      assertEquals(`Tabs hidden`, dismissEvent.detail.message);

      // Act.
      const restoreCallback = dismissEvent.detail.restoreCallback!;
      restoreCallback();
      assertTrue(!!moduleElement);
    });

    test(
        'Header dismiss button dispatches dismiss module event in pre ' +
            'redesign launchpad',
        async () => {
          loadTimeData.overrideValues({
            modulesRedesignedEnabled: false,
          });
          // Arrange.
          const moduleElement = await initializeModule(createSampleTabs(1));

          // Assert.
          assertTrue(!!moduleElement);
          const headerElement = $$(moduleElement, 'ntp-module-header');
          assertTrue(!!headerElement);
          const waitForDismissEvent =
              eventToPromise('dismiss-module', moduleElement);
          headerElement!.dispatchEvent(new Event('dismiss-button-click'));

          const dismissEvent: DismissModuleInstanceEvent =
              await waitForDismissEvent;
          assertEquals(`Tabs hidden`, dismissEvent.detail.message);

          // Act.
          const restoreCallback = dismissEvent.detail.restoreCallback!;
          restoreCallback();
          assertTrue(!!moduleElement);
        });

      test('Tab click fires usage event', async () => {
          // Arrange.
          const moduleElement = await initializeModule(createSampleTabs(1));

          // Assert.
          assertTrue(!!moduleElement);
          const tabElement = $$<HTMLElement>(moduleElement, '.tab');
          assertTrue(!!tabElement);
          const waitForUsageEvent =
              eventToPromise('usage', moduleElement);
          tabElement!.removeAttribute('href');
          tabElement!.click();
          assertEquals(
                1,
                metrics.count(`NewTabPage.TabResumption.ClickIndex`));

          await waitForUsageEvent;
        });
  });
});
