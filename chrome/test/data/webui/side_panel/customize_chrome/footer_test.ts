// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/footer.js';

import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote, ManagementNoticeState} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {FooterElement} from 'chrome://customize-chrome-side-panel.top-chrome/footer.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('FooterTest', () => {
  let footer: FooterElement;
  let metrics: MetricsTracker;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;

  setup(() => {
    metrics = fakeMetricsPrivate();
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    footer = document.createElement('customize-chrome-footer');
    document.body.appendChild(footer);
    return microtasksFinished();
  });

  async function setChecked(
      checked: boolean,
      managementNoticeState: ManagementNoticeState): Promise<void> {
    await handler.whenCalled('updateFooterSettings');
    callbackRouterRemote.setFooterSettings(
        checked, false, managementNoticeState);
    await callbackRouterRemote.$.flushForTesting();
  }

  ([true, false]).forEach((checked) => {
    test(`initial setting checked ${checked}`, async () => {
      await setChecked(checked, {canBeShown: false, enabledByPolicy: false});
      assertEquals(checked, footer.$.showToggle.checked);
    });
  });

  async function setManaged(managementNoticeState: ManagementNoticeState):
      Promise<void> {
    await handler.whenCalled('updateFooterSettings');
    callbackRouterRemote.setFooterSettings(true, false, managementNoticeState);
    await callbackRouterRemote.$.flushForTesting();
  }

  ([[true, false]] as Array<[boolean, boolean]>)
      .forEach(([noticeEnabledByPolicy]) => {
        const managementNoticeState = {
          canBeShown: true,
          enabledByPolicy: noticeEnabledByPolicy,
        };
        test(
            `initial setting managed by policy ${noticeEnabledByPolicy}`,
            async () => {
              await setManaged(managementNoticeState);
              assertEquals(noticeEnabledByPolicy, footer.$.showToggle.disabled);
              assertTrue(footer.$.showToggle.checked);
            });
      });

  async function setVisible(visible: boolean):
      Promise<void> {
    await handler.whenCalled('updateFooterSettings');
    callbackRouterRemote.setFooterSettings(visible, false, {canBeShown: true, enabledByPolicy: false});
    await callbackRouterRemote.$.flushForTesting();
  }

  ([true, false]).forEach((visible) => {
    test(`initial setting visible ${visible}`, async () => {
      await setVisible(visible);
      assertEquals(visible, footer.$.showToggle.checked);
    });
  });

  (['#showToggleContainer', '#showToggle']).forEach((selector: string) => {
    ([
      [
        false,
        'NewTabPage.Footer.ToggledVisibility.Consumer',
      ],
      [
        true,
        'NewTabPage.Footer.ToggledVisibility.Enterprise',
      ],
    ] as Array<[boolean, string]>)
        .forEach(([managementNoticeShown, histogramName]) => {
          test(
              `logs click metrics for ${selector} with management state :${
                  managementNoticeShown}`,
              async () => {
                const managementNoticeState = {
                  canBeShown: managementNoticeShown,
                  enabledByPolicy: false,
                };
                await setChecked(false, managementNoticeState);
                const toggle =
                    footer.shadowRoot.querySelector<HTMLElement>(selector);
                assertTrue(!!toggle);

                toggle.click();
                await microtasksFinished();

                assertEquals(
                    1,
                    metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
                assertEquals(
                    1,
                    metrics.count(
                        'NewTabPage.CustomizeChromeSidePanelAction',
                        CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED));
                assertEquals(1, metrics.count(histogramName));
                assertEquals(1, metrics.count(histogramName, true));
                assertEquals(
                    1, metrics.count('NewTabPage.Footer.ToggledVisibility'));
                assertEquals(
                    1,
                    metrics.count('NewTabPage.Footer.ToggledVisibility', true));

                toggle.click();
                await microtasksFinished();

                assertEquals(2, metrics.count(histogramName));
                assertEquals(1, metrics.count(histogramName, false));
                assertEquals(
                    2, metrics.count('NewTabPage.Footer.ToggledVisibility'));
                assertEquals(
                    1,
                    metrics.count(
                        'NewTabPage.Footer.ToggledVisibility', false));
              });
        });
  });

  (['#showToggleContainer', '#showToggle']).forEach((selector: string) => {
    test(`toggles visibility via ${selector}`, async () => {
      await setChecked(false, {canBeShown: false, enabledByPolicy: false});
      const toggle = $$<HTMLElement>(footer, selector);
      assertTrue(!!toggle);

      toggle.click();
      await microtasksFinished();

      assertTrue(footer.$.showToggle.checked);
      assertEquals(1, handler.getCallCount('setFooterVisible'));
      let visibleArg = handler.getArgs('setFooterVisible')[0];
      assertTrue(visibleArg);

      toggle.click();
      await microtasksFinished();

      assertFalse(footer.$.showToggle.checked);
      assertEquals(2, handler.getCallCount('setFooterVisible'));
      visibleArg = handler.getArgs('setFooterVisible')[1];
      assertFalse(visibleArg);
    });
  });
});
