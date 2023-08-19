// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';

import {ThemeColorPickerBrowserProxy} from 'chrome://resources/cr_components/theme_color_picker/browser_proxy.js';
import {ThemeColorPickerElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import {ThemeColorPickerClientCallbackRouter, ThemeColorPickerHandlerRemote} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('CrComponentsThemeColorPickerFocusTest', () => {
  let colorsElement: ThemeColorPickerElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const handler = TestMock.fromClass(ThemeColorPickerHandlerRemote);
    ThemeColorPickerBrowserProxy.setInstance(
        handler, new ThemeColorPickerClientCallbackRouter());
    handler.setResultFor('getChromeColors', new Promise(() => {}));
    colorsElement = document.createElement('cr-theme-color-picker');
    document.body.appendChild(colorsElement);
  });

  test('opens color picker', async () => {
    const focus = eventToPromise('focus', colorsElement.$.colorPicker);
    const click = eventToPromise('click', colorsElement.$.colorPicker);

    colorsElement.$.customColor.click();

    await focus;
    await click;
  });
});
