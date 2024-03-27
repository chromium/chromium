// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';

import {CustomizeColorSchemeModeBrowserProxy} from 'chrome://resources/cr_components/customize_color_scheme_mode/browser_proxy.js';
import type {ColorSchemeModeOption} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.js';
import {colorSchemeModeOptions, CustomizeColorSchemeModeElement} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.js';
import type {ColorSchemeMode, CustomizeColorSchemeModeClientRemote} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom-webui.js';
import {CustomizeColorSchemeModeClientCallbackRouter, CustomizeColorSchemeModeHandlerRemote} from 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    await microtasksFinished();
    return element;
  }

  colorSchemeModeOptions.forEach(
      (mode: ColorSchemeModeOption, index: number) => {
        test(`Set ${mode.id} on initialization`, async () => {
          // Arrange.
          const element = await initializeElement(mode.value);

          // Assert.
          // Check that the correct option is checked and the other aren't.
          colorSchemeModeOptions.forEach((innerMode: ColorSchemeModeOption) => {
            const checked = innerMode.id === mode.id;
            assertEquals(
                element.shadowRoot!
                    .querySelector(`segmented-button-option[name="${
                        innerMode.id}"]`)!.hasAttribute('checked'),
                checked);
          });
        });

        test(
            `sets scheme mode for ${mode.id} on-selected-changed`, async () => {
              // Arrange.
              const element = await initializeElement(
                  colorSchemeModeOptions.at(index - 1)!.value);

              // Action.
              const button =
                  element.shadowRoot!.querySelector('segmented-button');
              assertTrue(!!button);
              button.selected = mode.id;
              await microtasksFinished();

              // Assert.
              assertEquals(1, handler.getCallCount('setColorSchemeMode'));
              assertEquals(
                  handler.getArgs('setColorSchemeMode')[0], mode.value);
            });
      });
});
