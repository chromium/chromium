// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';

import type {ManagedDialogElement} from 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import {ThemeColorPickerBrowserProxy} from 'chrome://resources/cr_components/theme_color_picker/browser_proxy.js';
import type {Color} from 'chrome://resources/cr_components/theme_color_picker/color_utils.js';
import {DARK_BASELINE_BLUE_COLOR, DARK_BASELINE_GREY_COLOR, LIGHT_BASELINE_BLUE_COLOR, LIGHT_BASELINE_GREY_COLOR} from 'chrome://resources/cr_components/theme_color_picker/color_utils.js';
import type {ThemeColorElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color.js';
import type {ThemeColorPickerElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import type {ChromeColor, Theme, ThemeColorPickerClientRemote} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.mojom-webui.js';
import {ThemeColorPickerClientCallbackRouter, ThemeColorPickerHandlerRemote} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.mojom-webui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {BrowserColorVariant} from 'chrome://resources/mojo/ui/base/mojom/themes.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createTheme(isDarkMode = false): Theme {
  return {
    hasBackgroundImage: false,
    hasThirdPartyTheme: false,
    backgroundImageMainColor: null,
    isDarkMode,
    seedColor: {value: 0xff0000ff},
    seedColorHue: 0,
    backgroundColor: {value: 0xffff0000},
    foregroundColor: null,
    colorPickerIconColor: {value: 0xffff0000},
    colorsManagedByPolicy: false,
    isGreyBaseline: false,
    browserColorVariant: BrowserColorVariant.kTonalSpot,
    followDeviceTheme: false,
  };
}

suite('CrComponentsThemeColorPickerTest', () => {
  let colorsElement: ThemeColorPickerElement;
  let handler: TestMock<ThemeColorPickerHandlerRemote>&
      ThemeColorPickerHandlerRemote;
  let callbackRouter: ThemeColorPickerClientRemote;
  let chromeColorsResolver: PromiseResolver<{colors: ChromeColor[]}>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(ThemeColorPickerHandlerRemote);
    ThemeColorPickerBrowserProxy.setInstance(
        handler, new ThemeColorPickerClientCallbackRouter());
    callbackRouter = ThemeColorPickerBrowserProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
    chromeColorsResolver = new PromiseResolver();
  });

  function initializeElement() {
    handler.setResultFor('getChromeColors', chromeColorsResolver.promise);
    colorsElement = document.createElement('cr-theme-color-picker');
    document.body.appendChild(colorsElement);
  }

  ([
    [true, DARK_BASELINE_BLUE_COLOR],
    [false, LIGHT_BASELINE_BLUE_COLOR],
  ] as Array<[boolean, Color]>)
      .forEach(([isDarkMode, defaultColor]) => {
        test(
            `render DarkMode ${isDarkMode} default color`, async () => {
              initializeElement();
              const theme: Theme = createTheme(isDarkMode);

              callbackRouter.setTheme(theme);
              await callbackRouter.$.flushForTesting();

              const defaultColorElement =
                  $$<ThemeColorElement>(colorsElement, '#defaultColor')!;
              assertDeepEquals(
                  defaultColor.foreground, defaultColorElement.foregroundColor);
              assertDeepEquals(
                  defaultColor.background, defaultColorElement.backgroundColor);
              assertDeepEquals(
                  defaultColor.base, defaultColorElement.baseColor);
            });
      });

  ([
    [true, DARK_BASELINE_GREY_COLOR],
    [false, LIGHT_BASELINE_GREY_COLOR],
  ] as Array<[boolean, Color]>)
      .forEach(([isDarkMode, greyDefaultColor]) => {
        test(`render DarkMode ${isDarkMode} grey default color`, async () => {
          initializeElement();
          const theme: Theme = createTheme(isDarkMode);

          callbackRouter.setTheme(theme);
          await callbackRouter.$.flushForTesting();

          const greyDefaultColorElement =
              $$<ThemeColorElement>(colorsElement, '#greyDefaultColor')!;
          assertDeepEquals(
              greyDefaultColor.foreground,
              greyDefaultColorElement.foregroundColor);
          assertDeepEquals(
              greyDefaultColor.background,
              greyDefaultColorElement.backgroundColor);
          assertDeepEquals(
              greyDefaultColor.base, greyDefaultColorElement.baseColor);
        });
      });

  test('sets default color', async () => {
    initializeElement();
    const theme = createTheme();
    theme.foregroundColor = null;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    $$<HTMLElement>(colorsElement, '#defaultColor')!.click();

    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('sets grey default color', async () => {
    initializeElement();
    const theme = createTheme();
    theme.foregroundColor = null;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    $$<HTMLElement>(colorsElement, '#greyDefaultColor')!.click();

    assertEquals(1, handler.getCallCount('setGreyDefaultColor'));
  });

  test('renders chrome colors', async () => {
    const theme = createTheme();
    callbackRouter.setTheme(theme);
    initializeElement();
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 5},
          background: {value: 1},
          foreground: {value: 2},
          base: {value: 3},
          variant: BrowserColorVariant.kTonalSpot,
        },
        {
          id: 2,
          name: 'bar',
          seed: {value: 6},
          background: {value: 3},
          foreground: {value: 4},
          base: {value: 5},
          variant: BrowserColorVariant.kNeutral,
        },
      ],
    };

    chromeColorsResolver.resolve(colors);
    await microtasksFinished();

    const colorElements =
        colorsElement.shadowRoot!.querySelectorAll<ThemeColorElement>(
            '.chrome-color');
    assertEquals(2, colorElements.length);
    assertDeepEquals({value: 1}, colorElements[0]!.backgroundColor);
    assertDeepEquals({value: 2}, colorElements[0]!.foregroundColor);
    assertDeepEquals({value: 3}, colorElements[0]!.baseColor);
    assertEquals('foo', colorElements[0]!.title);
    assertDeepEquals({value: 3}, colorElements[1]!.backgroundColor);
    assertDeepEquals({value: 4}, colorElements[1]!.foregroundColor);
    assertDeepEquals({value: 5}, colorElements[1]!.baseColor);
    assertEquals('bar', colorElements[1]!.title);
  });

  test('sets chrome color', async () => {
    const theme = createTheme();
    callbackRouter.setTheme(theme);
    initializeElement();
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 3},
          background: {value: 1},
          foreground: {value: 2},
          base: {value: 4},
          variant: BrowserColorVariant.kNeutral,
        },
      ],
    };

    chromeColorsResolver.resolve(colors);
    await microtasksFinished();
    colorsElement.shadowRoot!.querySelector<ThemeColorElement>(
                                 '.chrome-color')!.click();

    const args = handler.getArgs('setSeedColor')[0];
    assertEquals(1, handler.getCallCount('setSeedColor'));
    assertEquals(3, args[0].value);
    assertEquals(BrowserColorVariant.kNeutral, args[1]);
  });

  test('sets custom color from hue slider', async () => {
    initializeElement();
    callbackRouter.setTheme(Object.assign(createTheme(), {seedColorHue: 10}));
    await callbackRouter.$.flushForTesting();

    colorsElement.$.hueSlider.selectedHue = 10;
    colorsElement.$.hueSlider.dispatchEvent(new Event('selected-hue-changed'));
    assertEquals(0, handler.getCallCount('setSeedColorFromHue'));

    colorsElement.$.hueSlider.selectedHue = 30;
    colorsElement.$.hueSlider.dispatchEvent(new Event('selected-hue-changed'));
    assertEquals(1, handler.getCallCount('setSeedColorFromHue'));
    assertEquals(30, handler.getArgs('setSeedColorFromHue')[0]);
  });

  test('updates custom color for theme', async () => {
    initializeElement();
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 3},
          background: {value: 1},
          foreground: {value: 2},
          base: {value: 3},
          variant: BrowserColorVariant.kTonalSpot,
        },
      ],
    };
    chromeColorsResolver.resolve(colors);

    // Set a custom color theme.
    const customColortheme = createTheme();
    customColortheme.seedColor = {value: 0xffffff00};
    customColortheme.backgroundColor = {value: 0xffff0000};
    customColortheme.foregroundColor = {value: 0xff00ff00};
    customColortheme.colorPickerIconColor = {value: 0xff0000ff};
    callbackRouter.setTheme(customColortheme);
    await callbackRouter.$.flushForTesting();

    // Custom color circle should be updated.
    assertEquals(0xffff0000, colorsElement.$.customColor.backgroundColor.value);
    assertEquals(0xff00ff00, colorsElement.$.customColor.foregroundColor.value);
    assertTrue(hasStyle(
        colorsElement.$.colorPickerIcon, 'background-color', 'rgb(0, 0, 255)'));

    // Set a theme that is not a custom color theme.
    const otherTheme = createTheme();
    otherTheme.seedColor = {value: 0xff00ff00};
    otherTheme.backgroundColor = {value: 0xffffffff};
    otherTheme.foregroundColor = null;  // Makes a default theme.
    otherTheme.colorPickerIconColor = {value: 0xffffffff};
    callbackRouter.setTheme(otherTheme);
    await callbackRouter.$.flushForTesting();

    // Custom color circle should be not be updated.
    assertEquals(0xffff0000, colorsElement.$.customColor.backgroundColor.value);
    assertEquals(0xff00ff00, colorsElement.$.customColor.foregroundColor.value);
    assertTrue(hasStyle(
        colorsElement.$.colorPickerIcon, 'background-color', 'rgb(0, 0, 255)'));
  });

  test('update colorPicker value for theme', async () => {
    initializeElement();
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 3},
          background: {value: 1},
          foreground: {value: 2},
          base: {value: 3},
          variant: BrowserColorVariant.kTonalSpot,
        },
      ],
    };
    chromeColorsResolver.resolve(colors);

    // Set a custom color theme.
    const customColortheme = createTheme();
    customColortheme.seedColorHue = 22;
    customColortheme.backgroundColor = {value: 0xffff0000};
    customColortheme.foregroundColor = {value: 0xff00ff00};
    customColortheme.colorPickerIconColor = {value: 0xff0000ff};
    callbackRouter.setTheme(customColortheme);
    await callbackRouter.$.flushForTesting();

    // Custom hue slider value should be updated.
    assertEquals(
        colorsElement.$.hueSlider.selectedHue, customColortheme.seedColorHue);

    // Set a theme that is not a custom color theme.
    const otherTheme = createTheme();
    otherTheme.backgroundColor = {value: 0xffffffff};
    otherTheme.foregroundColor = null;  // Makes a default theme.
    otherTheme.colorPickerIconColor = {value: 0xffffffff};
    callbackRouter.setTheme(otherTheme);
    await callbackRouter.$.flushForTesting();

    // Custom hue slider value should still be the same.
    assertEquals(
        colorsElement.$.hueSlider.selectedHue, customColortheme.seedColorHue);
  });

  test('checks selected color', async () => {
    initializeElement();
    const colors = {
      colors: [
        {
          id: 1,
          name: 'foo',
          seed: {value: 5},
          background: {value: 1},
          foreground: {value: 2},
          base: {value: 3},
          variant: BrowserColorVariant.kTonalSpot,
        },
        {
          id: 2,
          name: 'bar',
          seed: {value: 6},
          background: {value: 3},
          foreground: {value: 4},
          base: {value: 5},
          variant: BrowserColorVariant.kNeutral,
        },
      ],
    };
    chromeColorsResolver.resolve(colors);
    const theme = createTheme();

    // Set default color.
    theme.foregroundColor = null;
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check default color selected.
    const defaultColorElement =
        $$<ThemeColorElement>(colorsElement, '#defaultColor')!;
    let checkedColors =
        colorsElement.shadowRoot!.querySelectorAll<ThemeColorElement>(
            '[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals(defaultColorElement, checkedColors[0]);
    assertEquals(defaultColorElement.getAttribute('aria-checked'), 'true');
    let indexedColors =
        colorsElement.shadowRoot!.querySelectorAll('[tabindex="0"]');
    assertEquals(1, indexedColors.length);
    assertEquals(defaultColorElement, indexedColors[0]);

    // Set Chrome color.
    theme.seedColor = {value: 5};
    theme.foregroundColor = {value: 2};
    callbackRouter.setTheme(theme);
    await callbackRouter.$.flushForTesting();

    // Check Chrome color selected.
    checkedColors =
        colorsElement.shadowRoot!.querySelectorAll<ThemeColorElement>(
            '[checked]');
    assertEquals(1, checkedColors.length);
    assertEquals('chrome-color', checkedColors[0]!.className);
    assertEquals(checkedColors[0]!.getAttribute('aria-checked'), 'true');
    assertEquals(2, checkedColors[0]!.foregroundColor.value);
    assertEquals(3, checkedColors[0]!.baseColor!.value);
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

  [false, true].forEach(hasBackgroundImage => {
    test(
        `background color visible if theme has image ${hasBackgroundImage}`,
        async () => {
          initializeElement();
          const theme = createTheme();
          theme.hasBackgroundImage = hasBackgroundImage;
          callbackRouter.setTheme(theme);
          await callbackRouter.$.flushForTesting();

          const colors =
              colorsElement.shadowRoot!.querySelectorAll('cr-theme-color');
          for (const color of colors) {
            if (color.id === 'customColor') {
              assertTrue(color.backgroundColorHidden);
            } else {
              assertFalse(color.backgroundColorHidden);
            }
          }
        });
  });

  ([
    ['#defaultColor', null],
    ['.chrome-color', 3],
    ['#customColor', 10],
  ] as Array<[string, number]>)
      .forEach(([selector, foregroundColor]) => {
        test(`respects policy for ${selector}`, async () => {
          initializeElement();
          const colors = {
            colors: [
              {
                id: 1,
                name: 'foo',
                seed: {value: 3},
                background: {value: 1},
                foreground: {value: 2},
                base: {value: 3},
                variant: BrowserColorVariant.kTonalSpot,
              },
            ],
          };
          chromeColorsResolver.resolve(colors);
          const theme = createTheme();
          if (foregroundColor) {
            theme.foregroundColor = {value: foregroundColor};
          }
          theme.hasBackgroundImage = true;
          theme.colorsManagedByPolicy = true;
          callbackRouter.setTheme(theme);
          await callbackRouter.$.flushForTesting();

          $$<HTMLElement>(colorsElement, selector)!.click();
          await microtasksFinished();

          const managedDialog =
              $$<ManagedDialogElement>(colorsElement, 'managed-dialog');
          assertTrue(!!managedDialog);
          assertTrue(managedDialog.$.dialog.open);
          assertEquals(0, handler.getCallCount('setDefaultColor'));
          assertEquals(0, handler.getCallCount('setSeedColor'));
        });
      });
});
