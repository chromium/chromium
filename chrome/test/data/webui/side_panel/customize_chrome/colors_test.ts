// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {Color, DARK_DEFAULT_COLOR, LIGHT_DEFAULT_COLOR} from 'chrome://customize-chrome-side-panel.top-chrome/color_utils.js';
import {ColorsElement} from 'chrome://customize-chrome-side-panel.top-chrome/colors.js';
import {ChromeColor, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote, Theme} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ManagedDialogElement} from 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {$$, assertStyle, capture, createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('ColorsTest', () => {
  let colorsElement: ColorsElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
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
    handler.setResultFor(
        'getOverviewChromeColors', chromeColorsResolver.promise);
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
          await waitAfterNextRender(colorsElement);

          const defaultColorElement =
              $$<ColorElement>(colorsElement, '#defaultColor')!;
          assertDeepEquals(
              defaultColor.foreground, defaultColorElement.foregroundColor);
          assertDeepEquals(
              defaultColor.background, defaultColorElement.backgroundColor);
        });
      });

  test('sets default color', async () => {
    const theme = createTheme();
    theme.foregroundColor = undefined;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(colorsElement);

    $$<HTMLElement>(colorsElement, '#defaultColor')!.click();

    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('renders main color', async () => {
    const theme: Theme = createTheme();
    theme.foregroundColor = {value: 7};
    theme.backgroundImage = createBackgroundImage('https://foo.com');
    theme.backgroundImage.mainColor = {value: 7};

    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(colorsElement);

    assertEquals(
        7,
        $$<ColorElement>(colorsElement, '#mainColor')!.foregroundColor.value);
  });

  test('sets main color', async () => {
    const theme = createTheme();
    theme.foregroundColor = {value: 7};
    theme.backgroundImage = createBackgroundImage('https://foo.com');
    theme.backgroundImage.mainColor = {value: 7};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(colorsElement);

    $$<HTMLElement>(colorsElement, '#mainColor')!.click();

    assertEquals(1, handler.getCallCount('setSeedColor'));
    assertEquals(7, handler.getArgs('setSeedColor')[0].value);
  });

  test('renders chrome colors', async () => {
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 5},
          background: {value: 1},
          foreground: {value: 2},
        },
        {
          id: 2,
          name: 'bar',
          seed: {value: 6},
          background: {value: 3},
          foreground: {value: 4},
        },
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
        {
          id: 1,
          name: 'foo',
          seed: {value: 3},
          background: {value: 1},
          foreground: {value: 2},
        },
      ],
    };

    chromeColorsResolver.resolve(colors);
    await waitAfterNextRender(colorsElement);
    colorsElement.shadowRoot!.querySelector<ColorElement>(
                                 '.chrome-color')!.click();

    assertEquals(1, handler.getCallCount('setSeedColor'));
    assertEquals(3, handler.getArgs('setSeedColor')[0].value);
  });

  test('sets custom color', () => {
    colorsElement.$.colorPicker.value = '#ff0000';
    colorsElement.$.colorPicker.dispatchEvent(new Event('change'));

    const args = handler.getArgs('setSeedColor');
    assertGE(1, args.length);
    assertEquals(0xffff0000, args.at(-1).value);
  });

  test('updates custom color for theme', async () => {
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 3},
          background: {value: 1},
          foreground: {value: 2},
        },
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
        {
          id: 1,
          name: 'foo',
          seed: {value: 5},
          background: {value: 1},
          foreground: {value: 2},
        },
        {
          id: 2,
          name: 'bar',
          seed: {value: 6},
          background: {value: 3},
          foreground: {value: 4},
        },
      ],
    };
    chromeColorsResolver.resolve(colors);
    const theme = createTheme();

    // Set default color.
    theme.foregroundColor = undefined;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(colorsElement);

    // Check default color selected.
    const defaultColorElement =
        $$<ColorElement>(colorsElement, '#defaultColor')!;
    let checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(defaultColorElement, checkedColors[0]);
    assertEquals(defaultColorElement.getAttribute('aria-checked'), 'true');
    let indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals(defaultColorElement, indexedColors[0]);

    // Set main color.
    theme.seedColor = {value: 7};
    theme.foregroundColor = {value: 5};
    theme.backgroundImage = createBackgroundImage('https://foo.com');
    theme.backgroundImage.mainColor = {value: 7};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();
    await waitAfterNextRender(colorsElement);

    // Check main color selected.
    const mainColorElement = $$<ColorElement>(colorsElement, '#mainColor')!;
    checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(mainColorElement, checkedColors[0]);
    assertEquals(mainColorElement.getAttribute('aria-checked'), 'true');
    indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals(mainColorElement, indexedColors[0]);

    // Set Chrome color.
    theme.seedColor = {value: 5};
    theme.foregroundColor = {value: 2};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check Chrome color selected.
    checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals('chrome-color', checkedColors[0]!.className);
    assertEquals(checkedColors[0]!.getAttribute('aria-checked'), 'true');
    assertEquals(2, (checkedColors[0]! as ColorElement).foregroundColor.value);
    indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals('chrome-color', indexedColors[0]!.className);

    // Set custom color.
    theme.seedColor = {value: 10};
    theme.foregroundColor = {value: 5};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check custom color selected.
    checkedColors = colorsElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(colorsElement.$.customColor, checkedColors[0]);
    assertEquals(
        colorsElement.$.customColorContainer.getAttribute('aria-checked'),
        'true');
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

  ([
    ['#defaultColor', undefined, undefined],
    ['#mainColor', 7, 7],
    ['.chrome-color', 3, undefined],
    ['#customColor', 10, undefined],
  ] as Array<[string, number?, number?]>).forEach(
  ([selector, foregroundColor, mainColor]) => {
    test(`respects policy for ${selector}`, async () => {
      const colors = {
        colors: [
          {
            id: 1,
            name: 'foo',
            seed: {value: 3},
            background: {value: 1},
            foreground: {value: 2},
          },
        ],
      };
      chromeColorsResolver.resolve(colors);
      const theme = createTheme();
      if (foregroundColor) {
        theme.foregroundColor = {value: foregroundColor};
      }
      theme.backgroundImage = createBackgroundImage('https://foo.com');
      if (mainColor) {
        theme.backgroundImage.mainColor = {value: mainColor};
      }
      theme.colorsManagedByPolicy = true;
      callbackRouter.setTheme(theme);
      await callbackRouter.$.flushForTesting();
      await waitAfterNextRender(colorsElement);
      const click = capture(colorsElement.$.colorPicker, 'click');

      $$<HTMLElement>(colorsElement, selector)!.click();
      await waitAfterNextRender(colorsElement);

      const managedDialog =
          $$<ManagedDialogElement>(colorsElement, 'managed-dialog');
      assertTrue(!!managedDialog);
      assertTrue(managedDialog.$.dialog.open);
      assertEquals(0, handler.getCallCount('setDefaultColor'));
      assertEquals(0, handler.getCallCount('setSeedColor'));
      assertFalse(click.received);
    });
  });
});
