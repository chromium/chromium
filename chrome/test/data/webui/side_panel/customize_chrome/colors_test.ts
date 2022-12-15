// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {Color, ColorsElement, DARK_DEFAULT_COLOR, LIGHT_DEFAULT_COLOR} from 'chrome://customize-chrome-side-panel.top-chrome/colors.js';
import {ChromeColor, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote, Theme} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {assertStyle, capture, createBackgroundImage, createTheme, installMock} from './test_support.js';

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
          const theme: Theme = createTheme(systemDarkMode);

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

  test('opens color picker', () => {
    const focus = capture(colorsElement.$.colorPicker, 'focus');
    const click = capture(colorsElement.$.colorPicker, 'click');

    colorsElement.$.customColor.click();

    assertTrue(focus.received);
    assertTrue(click.received);
  });

  test('sets custom color', () => {
    colorsElement.$.colorPicker.value = '#ff0000';
    colorsElement.$.colorPicker.dispatchEvent(new Event('change'));

    const args = handler.getArgs('setForegroundColor');
    assertGE(1, args.length);
    assertEquals(0xffff0000, args.at(-1).value);
  });

  test('updates custom color for theme', async () => {
    const colors = {
      colors: [
        {id: 1, name: 'foo', background: {value: 1}, foreground: {value: 2}},
      ],
    };
    chromeColorsResolver.resolve(colors);

    // Set a custom color theme.
    const customColortheme = createTheme();
    customColortheme.backgroundColor = {value: 0xffff0000};
    customColortheme.foregroundColor = {value: 0xff00ff00};
    customColortheme.colorPickerIconColor = {value: 0xff0000ff};
    callbackRouter.setTheme(customColortheme);
    await callbackRouter.$.flushForTesting();

    // Custom color circle should be updated.
    assertEquals(0xffff0000, colorsElement.$.customColor.backgroundColor.value);
    assertEquals(0xff00ff00, colorsElement.$.customColor.foregroundColor.value);
    assertStyle(
        colorsElement.$.colorPickerIcon, 'background-color', 'rgb(0, 0, 255)');

    // Set a theme that is not a custom color theme.
    const otherTheme = createTheme();
    otherTheme.backgroundColor = {value: 0xffffffff};
    otherTheme.foregroundColor = undefined;  // Makes a default theme.
    otherTheme.colorPickerIconColor = {value: 0xffffffff};
    callbackRouter.setTheme(otherTheme);
    await callbackRouter.$.flushForTesting();

    // Custom color circle should be not be updated.
    assertEquals(0xffff0000, colorsElement.$.customColor.backgroundColor.value);
    assertEquals(0xff00ff00, colorsElement.$.customColor.foregroundColor.value);
    assertStyle(
        colorsElement.$.colorPickerIcon, 'background-color', 'rgb(0, 0, 255)');
  });

  test('checks selected color', async () => {
    const colors = {
      colors: [
        {id: 1, name: 'foo', background: {value: 1}, foreground: {value: 2}},
        {id: 2, name: 'bar', background: {value: 3}, foreground: {value: 4}},
      ],
    };
    chromeColorsResolver.resolve(colors);
    const theme = createTheme();

    // Set default color.
    theme.foregroundColor = undefined;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check default color selected.
    let checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(colorsElement.$.defaultColor, checkedColors[0]);
    let indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals(colorsElement.$.defaultColor, indexedColors[0]);

    // Set Chrome color.
    theme.foregroundColor = {value: 2};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check Chrome color selected.
    checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals('chrome-color', checkedColors[0]!.className);
    assertEquals(2, (checkedColors[0]! as ColorElement).foregroundColor.value);
    indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals('chrome-color', indexedColors[0]!.className);

    // Set custom color.
    theme.foregroundColor = {value: 5};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check custom color selected.
    checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(colorsElement.$.customColor, checkedColors[0]);
    indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals(colorsElement.$.customColorContainer, indexedColors[0]);
  });

  [false, true].forEach((hasBackgroundImage) => {
    test(
        `background color visibility if theme has image ${hasBackgroundImage}`,
        async () => {
          const theme = createTheme();
          if (hasBackgroundImage) {
            theme.backgroundImage = createBackgroundImage('https://foo.com');
          } else {
            theme.backgroundImage = undefined;
          }
          callbackRouter.setTheme(theme);
          await callbackRouter.$.flushForTesting();

          const colors = colorsElement.shadowRoot!.querySelectorAll(
              'customize-chrome-color');
          for (const color of colors) {
            assertEquals(hasBackgroundImage, color.backgroundColorHidden);
          }
        });
  });
});
