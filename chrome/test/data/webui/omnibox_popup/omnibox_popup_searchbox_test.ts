// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupPageRemote, OmniboxPopupSearchboxElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxPopupSearchboxTest', function() {
  let searchbox: OmniboxPopupSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let callbackRouter: OmniboxPopupPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    handler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
    const {instance, remote} =
        omniboxPopupBrowserProxyFactory.createForTest(handler);
    callbackRouter = remote;
    omniboxPopupBrowserProxyFactory.setInstance(instance);
    searchbox = document.createElement('omnibox-popup-searchbox');
    document.body.appendChild(searchbox);
    await microtasksFinished();
  });

  test('HandlesSetInputText', async () => {
    const testText = 'test input';

    // Simulate receiving the Mojo event
    callbackRouter.setInputText(testText);
    await microtasksFinished();

    // Verify that the input element was updated.
    const lastInput = searchbox.$.input.lastInput();
    assertTrue(!!lastInput);
    assertEquals(testText, lastInput.text);
  });
});
