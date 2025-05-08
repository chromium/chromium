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
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

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

  test('clicking footer toggle sets metric', async () => {
    const toggle = footer.$.showToggle;
    toggle.click();
    await microtasksFinished();

    assertEquals(1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.CustomizeChromeSidePanelAction',
            CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED));
  });

  test('turning toggle on updates footer visibility', async () => {
    // Start with toggle off.
    await handler.whenCalled('updateFooterSettings');
    callbackRouterRemote.setFooterSettings(false);
    await callbackRouterRemote.$.flushForTesting();
    assertFalse(footer.$.showToggle.checked);

    // Turn toggle on.
    footer.$.showToggle.click();
    await microtasksFinished();
    assertEquals(1, handler.getCallCount('setFooterVisible'));
    assertTrue(footer.$.showToggle.checked);
    const footerVisible = handler.getArgs('setFooterVisible')[0];
    assertTrue(footerVisible);
  });

  test('turning toggle off updates footer visibility', async () => {
    // Start with toggle on.
    await handler.whenCalled('updateFooterSettings');
    callbackRouterRemote.setFooterSettings(true);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(footer.$.showToggle.checked);

    // Turn toggle off.
    footer.$.showToggle.click();
    await microtasksFinished();
    assertEquals(1, handler.getCallCount('setFooterVisible'));
    assertFalse(footer.$.showToggle.checked);
    const footerVisible = handler.getArgs('setFooterVisible')[0];
    assertFalse(footerVisible);
  });
});
