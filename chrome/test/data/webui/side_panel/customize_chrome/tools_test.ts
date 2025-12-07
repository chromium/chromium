// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/tools.js';

import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {ToolChipsElement} from 'chrome://customize-chrome-side-panel.top-chrome/tools.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

// Note that the following test suite only tests the interaction from the WebUI
// to the customize chrome page handler backend. This test suite is not
// responsible for testing the backend logic for synchronizing the pref in the
// WebUI, as that's done through the backend unit tests.
suite('ToolChipsTest', () => {
  let toolChips: ToolChipsElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;

  setup(() => {
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolChips = document.createElement('customize-chrome-tools');
    document.body.appendChild(toolChips);
    return microtasksFinished();
  });

  // Mock function to simulate a call to the customize chrome page handler.
  async function setIsChipsEnabled(isEnabled: boolean): Promise<void> {
    await handler.whenCalled('updateToolChipsSettings');
    callbackRouterRemote.setToolsSettings(isEnabled);
    await callbackRouterRemote.$.flushForTesting();
  }

  // Tests that the mocked Mojo call changes the state of the toggle in the DOM.
  ([true, false]).forEach((isChipsEnabled) => {
    test(`initial setting checked ${isChipsEnabled}`, async () => {
      await setIsChipsEnabled(isChipsEnabled);
      assertEquals(isChipsEnabled, toolChips.$.showChipsToggle.checked);
    });
  });

  // Test that checks if the toggle is functioning as expected, and simulates
  // a flip/flop action of the toggle; checks arguments passed and DOM state.
  (['#showToggleContainer', '#showChipsToggle']).forEach((selector: string) => {
    test(`toggles chips visibility via ${selector}`, async () => {
      await setIsChipsEnabled(false);
      const toggle = $$<HTMLElement>(toolChips, selector);
      assertTrue(!!toggle);

      toggle.click();
      await microtasksFinished();

      assertTrue(toolChips.$.showChipsToggle.checked);
      assertEquals(1, handler.getCallCount('setToolChipsVisible'));
      let visibleArg = handler.getArgs('setToolChipsVisible')[0];
      assertTrue(visibleArg);

      toggle.click();
      await microtasksFinished();

      assertFalse(toolChips.$.showChipsToggle.checked);
      assertEquals(2, handler.getCallCount('setToolChipsVisible'));
      visibleArg = handler.getArgs('setToolChipsVisible')[1];
      assertFalse(visibleArg);
    });
  });
});
