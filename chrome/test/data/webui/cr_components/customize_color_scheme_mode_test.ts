// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CustomizeColorSchemeModeBrowserProxy} from 'chrome://resources/cr_components/customize_color_scheme_mode/browser_proxy.js';
import {ColorSchemeModeOption, colorSchemeModeOptions, CustomizeColorSchemeModeElement} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.js';
import {ColorSchemeMode, CustomizeColorSchemeModeClientCallbackRouter, CustomizeColorSchemeModeClientRemote, CustomizeColorSchemeModeHandlerRemote} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('CrComponentsCustomizeColorSchemeModeTest', () => {
  let handler: TestMock<CustomizeColorSchemeModeHandlerRemote>&
      CustomizeColorSchemeModeHandlerRemote;
  let callbackRouter: CustomizeColorSchemeModeClientRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(CustomizeColorSchemeModeHandlerRemote);
    CustomizeColorSchemeModeBrowserProxy.setInstance(
        handler, new CustomizeColorSchemeModeClientCallbackRouter());
    callbackRouter = CustomizeColorSchemeModeBrowserProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function initializeElement(colorSchemeMode: ColorSchemeMode):
      Promise<CustomizeColorSchemeModeElement> {
    const element = new CustomizeColorSchemeModeElement();
    callbackRouter.setColorSchemeMode(colorSchemeMode);
    document.body.appendChild(element);
    await waitAfterNextRender(element);
    return element;
  }

  colorSchemeModeOptions.forEach((mode: ColorSchemeModeOption) => {
    test(`Set ${mode.id} on initialization`, async () => {
      // Arrange.
      const element = await initializeElement(mode.value);

      // Assert.
      // Check that the correct option is checked and the other aren't.
      colorSchemeModeOptions.forEach((innerMode: ColorSchemeModeOption) => {
        const checked = innerMode.id === mode.id;
        assertEquals(
            element.shadowRoot!
                .querySelector(`cr-segmented-button-option[name="${
                    innerMode.id}"]`)!.hasAttribute('checked'),
            checked);
      });
    });

    test(`Click ${mode.id} sets scheme mode`, async () => {
      // Arrange.
      const element = await initializeElement(mode.value);

      // Action.
      const option =
          element.shadowRoot!.querySelector(
              `cr-segmented-button-option[name="${mode.id}"]`)! as HTMLElement;
      option.click();

      // Assert.
      assertEquals(1, handler.getCallCount('setColorSchemeMode'));
    });
  });
});
