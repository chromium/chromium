// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/strings.m.js';

import {ThemeColorPickerBrowserProxy} from 'chrome://resources/cr_components/theme_color_picker/browser_proxy.js';
import {ThemeColorPickerElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import {ThemeColorPickerClientCallbackRouter, ThemeColorPickerHandlerRemote} from 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {capture, installMock} from './test_support.js';

suite('CrComponentsThemeColorPickerFocusTest', () => {
  let colorsElement: ThemeColorPickerElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const handler = installMock(
        ThemeColorPickerHandlerRemote,
        (mock: ThemeColorPickerHandlerRemote) =>
            ThemeColorPickerBrowserProxy.setInstance(
                mock, new ThemeColorPickerClientCallbackRouter()));
    handler.setResultFor('getChromeColors', new Promise(() => {}));
    colorsElement = new ThemeColorPickerElement();
    document.body.appendChild(colorsElement);
  });

  test('opens color picker', () => {
    const focus = capture(colorsElement.$.colorPicker, 'focus');
    const click = capture(colorsElement.$.colorPicker, 'click');

    colorsElement.$.customColor.click();

    assertTrue(focus.received);
    assertTrue(click.received);
  });
});
