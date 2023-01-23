// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorsElement} from 'chrome://customize-chrome-side-panel.top-chrome/colors.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {capture, installMock} from './test_support.js';

suite('ColorsFocusTest', () => {
  let colorsElement: ColorsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    handler.setResultFor('getOverviewChromeColors', new Promise(() => {}));
    colorsElement = new ColorsElement();
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
