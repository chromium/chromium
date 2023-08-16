// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';

import {ChromeColorsElement} from 'chrome://customize-chrome-side-panel.top-chrome/chrome_colors.js';
import {ThemeColorPickerBrowserProxy} from 'chrome://resources/cr_components/theme_color_picker/browser_proxy.js';
import {ThemeColorElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color.js';
import {ChromeColor, Theme, ThemeColorPickerClientCallbackRouter, ThemeColorPickerClientRemote, ThemeColorPickerHandlerRemote} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.mojom-webui.js';
import {BrowserColorVariant} from 'chrome://resources/mojo/ui/base/mojom/themes.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

function createTheme(): Theme {
  return {
    hasBackgroundImage: false,
    hasThirdPartyTheme: false,
    backgroundImageMainColor: undefined,
    isDarkMode: false,
    seedColor: {value: 0xff0000ff},
    backgroundColor: {value: 0xffff0000},
    foregroundColor: undefined,
    colorPickerIconColor: {value: 0xffff0000},
    colorsManagedByPolicy: false,
    isGreyBaseline: false,
    browserColorVariant: BrowserColorVariant.kTonalSpot,
    followDeviceTheme: false,
  };
}

suite('ChromeColorsTest', () => {
  let chromeColorsElement: ChromeColorsElement;
  let handler: TestMock<ThemeColorPickerHandlerRemote>;
  let callbackRouter: ThemeColorPickerClientRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        ThemeColorPickerHandlerRemote,
        (mock: ThemeColorPickerHandlerRemote) =>
            ThemeColorPickerBrowserProxy.setInstance(
                mock, new ThemeColorPickerClientCallbackRouter()));
    callbackRouter = ThemeColorPickerBrowserProxy.getInstance()
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
        base: {value: i + 3},
        variant: BrowserColorVariant.kTonalSpot,
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
    chromeColorsElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('get chrome colors', async () => {
    await setInitialSettings(2);

    const colors =
        chromeColorsElement.shadowRoot!.querySelectorAll<ThemeColorElement>(
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
        .querySelector<ThemeColorElement>('.chrome-color')!.click();

    // Should remove background image if there is one.
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    const args = handler.getArgs('setSeedColor')[0];
    assertEquals(1, handler.getCallCount('setSeedColor'));
    assertEquals(0, args[0].value);
    assertEquals(BrowserColorVariant.kTonalSpot, args[1]);
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
    const args = handler.getArgs('setSeedColor')[0];
    assertEquals(2, args.length);
    assertEquals(0xffff0000, args[0].value);
    assertEquals(BrowserColorVariant.kTonalSpot, args[1]);
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
    assertEquals(
        3, (checkedColors[0]! as ThemeColorElement).foregroundColor.value);
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
    theme.hasThirdPartyTheme = true;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(chromeColorsElement);

    // Check that no color is selected.
    checkedColors =
        chromeColorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(0, checkedColors.length);
  });
});
