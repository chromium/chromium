// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/footer.js';

import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
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

  async function setChecked(checked: boolean): Promise<void> {
    await handler.whenCalled('updateFooterSettings');
    callbackRouterRemote.setFooterSettings(checked);
    await callbackRouterRemote.$.flushForTesting();
  }

  ([true, false]).forEach((checked) => {
    test(`initial setting ${checked}`, async () => {
      await setChecked(checked);
      assertEquals(checked, footer.$.showToggle.checked);
    });
  });

  (['#showToggleContainer', '#showToggle']).forEach((selector: string) => {
    test(`logs click metrics for ${selector}`, async () => {
      await setChecked(false);
      const toggle = footer.shadowRoot.querySelector<HTMLElement>(selector);
      assertTrue(!!toggle);

      toggle.click();
      await microtasksFinished();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED));
      assertEquals(1, metrics.count('NewTabPage.Footer.ToggledVisibility'));
      assertEquals(
          1, metrics.count('NewTabPage.Footer.ToggledVisibility', true));

      toggle.click();
      await microtasksFinished();

      assertEquals(2, metrics.count('NewTabPage.Footer.ToggledVisibility'));
      assertEquals(
          1, metrics.count('NewTabPage.Footer.ToggledVisibility', false));
    });
  });

  (['#showToggleContainer', '#showToggle']).forEach((selector: string) => {
    test(`toggles visibility via ${selector}`, async () => {
      await setChecked(false);
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
