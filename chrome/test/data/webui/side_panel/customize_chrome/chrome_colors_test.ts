// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';

import {ChromeColorsElement} from 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';
import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {ChromeColor, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {assertDeepEquals, assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTheme, installMock} from './test_support.js';

suite('ChromeColorsTest', () => {
  let chromeColorsElement: ChromeColorsElement;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;
  let callbackRouter: CustomizeChromePageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouter = CustomizeChromeApiProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
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

  test('checks selected color', async () => {
    await setInitialSettings(2);
    const theme = createTheme();

    // Set default color.
    theme.foregroundColor = undefined;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(chromeColorsElement);

    // Check default color selected.
    const defaultColorElement = chromeColorsElement.$.defaultColor;
    let checkedColors =
        chromeColorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(defaultColorElement, checkedColors[0]);
    assertEquals('true', checkedColors[0]!.getAttribute('aria-current'));

    // Set Chrome color.
    theme.seedColor = {value: 1};
    theme.foregroundColor = {value: 3};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(chromeColorsElement);

    // Check Chrome color selected.
    checkedColors =
        chromeColorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals('chrome-color tile', checkedColors[0]!.className);
    assertEquals(3, (checkedColors[0]! as ColorElement).foregroundColor.value);
    assertEquals('true', checkedColors[0]!.getAttribute('aria-current'));

    // Set custom color.
    theme.seedColor = {value: 10};
    theme.foregroundColor = {value: 5};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(chromeColorsElement);

    // Check custom color selected.
    checkedColors =
        chromeColorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(chromeColorsElement.$.customColor, checkedColors[0]);
    assertEquals(
        'true', checkedColors[0]!.parentElement!.getAttribute('aria-current'));

    // Set a CWS theme.
    theme.thirdPartyThemeInfo = {
      id: '123',
      name: 'test',
    };
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(chromeColorsElement);

    // Check that no color is selected.
    checkedColors =
        chromeColorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(0, checkedColors.length);
  });
});
