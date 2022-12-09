// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {Color, ColorsElement, DARK_DEFAULT_COLOR, LIGHT_DEFAULT_COLOR} from 'chrome://customize-chrome-side-panel.top-chrome/colors.js';
import {ChromeColor, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote, Theme} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from './test_support.js';

suite('ColorsTest', () => {
  let colorsElement: ColorsElement;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;
  let callbackRouter: CustomizeChromePageRemote;
  let chromeColorsResolver: PromiseResolver<{colors: ChromeColor[]}>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouter = CustomizeChromeApiProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
    chromeColorsResolver = new PromiseResolver();
    handler.setResultFor('getChromeColors', chromeColorsResolver.promise);
    colorsElement = new ColorsElement();
    document.body.appendChild(colorsElement);
  });

  ([
    [true, DARK_DEFAULT_COLOR],
    [false, LIGHT_DEFAULT_COLOR],
  ] as Array<[boolean, Color]>)
      .forEach(([systemDarkMode, defaultColor]) => {
        test('renders default color', async () => {
          const theme: Theme = {
            backgroundImage: undefined,
            systemDarkMode,
            foregroundColor: undefined,
          };

          callbackRouter.setTheme(theme);
          await callbackRouter.$.flushForTesting();

          assertDeepEquals(
              defaultColor.foreground,
              colorsElement.$.defaultColor.foregroundColor);
          assertDeepEquals(
              defaultColor.background,
              colorsElement.$.defaultColor.backgroundColor);
        });
      });

  test('sets default color', () => {
    colorsElement.$.defaultColor.click();

    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('renders chrome colors', async () => {
    const colors = {
      colors: [
        {id: 1, name: 'foo', background: {value: 1}, foreground: {value: 2}},
        {id: 2, name: 'bar', background: {value: 3}, foreground: {value: 4}},
      ],
    };

    chromeColorsResolver.resolve(colors);
    await waitAfterNextRender(colorsElement);

    const colorElements =
        colorsElement.shadowRoot!.querySelectorAll<ColorElement>(
            '.chrome-color');
    assertEquals(2, colorElements.length);
    assertDeepEquals({value: 1}, colorElements[0]!.backgroundColor);
    assertDeepEquals({value: 2}, colorElements[0]!.foregroundColor);
    assertEquals('foo', colorElements[0]!.title);
    assertDeepEquals({value: 3}, colorElements[1]!.backgroundColor);
    assertDeepEquals({value: 4}, colorElements[1]!.foregroundColor);
    assertEquals('bar', colorElements[1]!.title);
  });

  test('sets chrome color', async () => {
    const colors = {
      colors: [
        {id: 1, name: 'foo', background: {value: 1}, foreground: {value: 2}},
      ],
    };

    chromeColorsResolver.resolve(colors);
    await waitAfterNextRender(colorsElement);
    colorsElement.shadowRoot!.querySelector<ColorElement>(
                                 '.chrome-color')!.click();

    assertEquals(1, handler.getCallCount('setForegroundColor'));
    assertEquals(2, handler.getArgs('setForegroundColor')[0].value);
  });
});
