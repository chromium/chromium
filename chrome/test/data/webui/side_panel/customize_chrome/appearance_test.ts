// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';

import {AppearanceElement} from 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('AppearanceTest', () => {
  let appearanceElement: AppearanceElement;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    // Set result for getChromeColors for the mock handler. Otherwise, it
    // crashes since colors is a child of appearance and needs a Promise
    // from getChromeColors.
    handler.setResultFor('getChromeColors', Promise.resolve([]));
    appearanceElement = document.createElement('customize-chrome-appearance');
    document.body.appendChild(appearanceElement);
    await handler.whenCalled('getChromeColors');
  });

  test('appearance edit button creates event', async () => {
    const eventPromise = eventToPromise('edit-theme-click', appearanceElement);
    appearanceElement.$.editThemeButton.click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('classic chrome button shows with background image', async () => {
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    assertTrue(!appearanceElement.$.setClassicChromeButton.hidden);
  });

  test('classic chrome button does not show with classic chrome', async () => {
    const theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    assertTrue(appearanceElement.$.setClassicChromeButton.hidden);
  });

  test('classic chrome button sets theme to classic chrome', async () => {
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    appearanceElement.$.setClassicChromeButton.click();
    assertEquals(1, handler.getCallCount('setClassicChromeDefaultTheme'));
  });
});
