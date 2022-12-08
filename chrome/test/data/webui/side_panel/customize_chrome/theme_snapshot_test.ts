// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/theme_snapshot.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ThemeSnapshotElement} from 'chrome://customize-chrome-side-panel.top-chrome/theme_snapshot.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {$$, createBackgroundImage, createTheme, installMock} from './test_support.js';


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

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    assertEquals(
        'chrome://theme/foo',
        $$<HTMLImageElement>(themeSnapshotElement, '#themeSnapshot img')!.src);
  });
});
