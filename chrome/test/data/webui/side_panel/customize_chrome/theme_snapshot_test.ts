// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/theme_snapshot.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CustomizeThemeType, ThemeSnapshotElement} from 'chrome://customize-chrome-side-panel.top-chrome/theme_snapshot.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {$$, assertStyle, createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('ThemeSnapshotTest', () => {
  let themeSnapshotElement: ThemeSnapshotElement;
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
  });

  async function createThemeSnapshotElement(): Promise<void> {
    handler.setResultFor('updateTheme', Promise.resolve({
      theme: null,
    }));
    themeSnapshotElement =
        document.createElement('customize-chrome-theme-snapshot');
    document.body.appendChild(themeSnapshotElement);
    await handler.whenCalled('updateTheme');
  }

  test('setting theme updates theme snapshot', async () => {
    // Arrange.
    createThemeSnapshotElement();
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');
    theme.backgroundImage.title = 'foo';

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    const shownPages =
        themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
    assertTrue(!!shownPages);
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('theme-type'),
        CustomizeThemeType.CUSTOM_THEME);
    assertEquals(
        $$<HTMLImageElement>(
            themeSnapshotElement, '.theme-snapshot #customThemeImage')!
            .getAttribute('aria-labelledby'),
        'customThemeTitle');
    assertEquals(
        'foo',
        $$(themeSnapshotElement,
           '.theme-snapshot #customThemeTitle')!.textContent!.trim());
    assertEquals(
        'chrome://theme/foo',
        $$<HTMLImageElement>(themeSnapshotElement, '.theme-snapshot img')!.src);
  });

  test('not setting a theme updates preview background color', async () => {
    // Arrange.
    createThemeSnapshotElement();
    const theme = createTheme();
    theme.backgroundColor = {value: 4279522202};

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    const shownPages =
        themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
    assertTrue(!!shownPages);
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('theme-type'),
        CustomizeThemeType.CLASSIC_CHROME);
    assertEquals(
        $$<HTMLImageElement>(
            themeSnapshotElement,
            '.theme-snapshot #miniNewTabPage')!.getAttribute('aria-labelledby'),
        'classicChromeThemeTitle');
    assertEquals(
        'Classic Chrome',
        $$(themeSnapshotElement,
           '.theme-snapshot #classicChromeThemeTitle')!.textContent!.trim());
    assertStyle(
        $$(themeSnapshotElement, '.theme-snapshot #classicChrome')!,
        'background-color', 'rgb(20, 83, 154)');
  });

  test('uploading a background updates theme snapshot', async () => {
    // Arrange.
    createThemeSnapshotElement();
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');
    theme.backgroundImage.isUploadedImage = true;

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    const shownPages =
        themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
    assertTrue(!!shownPages);
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('theme-type'),
        CustomizeThemeType.UPLOADED_IMAGE);
    assertEquals(
        $$(themeSnapshotElement, '.theme-snapshot #uploadedThemeImage')!
            .getAttribute('aria-labelledby'),
        'uploadedThemeTitle');
    assertEquals(
        'Uploaded image',
        $$(themeSnapshotElement,
           '.theme-snapshot #uploadedThemeTitle')!.textContent!.trim());
  });
});
