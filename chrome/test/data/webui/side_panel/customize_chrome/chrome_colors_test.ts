// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';

import {ChromeColorsElement} from 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';
import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {ChromeColor, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {assertDeepEquals, assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  test('sets chrome color', async () => {
    await setInitialSettings(1);

    chromeColorsElement.shadowRoot!
        .querySelector<ColorElement>('.chrome-color')!.click();

    // Should remove background image if there is one.
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    assertEquals(1, handler.getCallCount('setSeedColor'));
    assertEquals(0, handler.getArgs('setSeedColor')[0].value);
  });

  test('sets default color', async () => {
    await setInitialSettings(1);

    chromeColorsElement.$.defaultColor.click();

    // Should remove background image if there is one.
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('sets custom color', async () => {
    await setInitialSettings(0);
    chromeColorsElement.$.colorPicker.value = '#ff0000';
    chromeColorsElement.$.colorPicker.dispatchEvent(new Event('change'));

    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    const args = handler.getArgs('setSeedColor');
    assertGE(1, args.length);
    assertEquals(0xffff0000, args.at(-1).value);
  });
});
