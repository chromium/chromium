// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';

import {ChromeColorsElement} from 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';
import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {ChromeColor, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('ChromeColorsTest', () => {
  let chromeColorsElement: ChromeColorsElement;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
  });

  async function setInitialSettings(numColors: number): Promise<void> {
    const colors: ChromeColor[] = [];
    for (let i = 0; i < numColors; i++) {
      colors.push({
        name: `color_${i}`,
        seed: {value: i},
        background: {value: i + 1},
        foreground: {value: i + 2},
      });
    }
    handler.setResultFor('getChromeColors', Promise.resolve({colors}));
    chromeColorsElement =
        document.createElement('customize-chrome-chrome-colors');
    document.body.appendChild(chromeColorsElement);
    await handler.whenCalled('getChromeColors');
  }

  test('back button create event', async () => {
    await setInitialSettings(0);

    const eventPromise = eventToPromise('back-click', chromeColorsElement);
    chromeColorsElement.$.backButton.click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('get chrome colors', async () => {
    await setInitialSettings(2);

    const colors =
        chromeColorsElement.shadowRoot!.querySelectorAll<ColorElement>(
            '.chrome-color');
    assertEquals(colors.length, 2);
    assertDeepEquals({value: 1}, colors[0]!.backgroundColor);
    assertDeepEquals({value: 2}, colors[0]!.foregroundColor);
    assertEquals('color_0', colors[0]!.title);
    assertDeepEquals({value: 2}, colors[1]!.backgroundColor);
    assertDeepEquals({value: 3}, colors[1]!.foregroundColor);
    assertEquals('color_1', colors[1]!.title);
  });
});
