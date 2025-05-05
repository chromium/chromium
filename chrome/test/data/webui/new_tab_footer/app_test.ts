// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://newtab-footer/app.js';

import type {NewTabFooterAppElement} from 'chrome://newtab-footer/app.js';
import {NewTabFooterDocumentProxy} from 'chrome://newtab-footer/browser_proxy.js';
import {NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabFooterAppTest', () => {
  let element: NewTabFooterAppElement;
  let handler: TestMock<NewTabFooterHandlerRemote>&NewTabFooterHandlerRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(NewTabFooterHandlerRemote);
    NewTabFooterDocumentProxy.setInstance(
        handler, new NewTabFooterDocumentCallbackRouter());
  });

  async function initializeElement() {
    element = document.createElement('new-tab-footer-app');
    document.body.appendChild(element);
    await microtasksFinished();
  }

  test('Get extension attibution on initialization', async () => {
    // Arrange.
    handler.setResultFor(
        'getNtpExtensionAttribution',
        {attribution: {name: 'foo', url: 'chrome://extensions/?id=1234'}});
    await initializeElement();

    // Assert.
    const attribution =
        element.shadowRoot.querySelector('#extensionAttribution');
    assertTrue(!!attribution);
    const attributionLink = attribution.querySelector('a');
    assertEquals(attributionLink!.href, 'chrome://extensions/?id=1234');
    assertEquals(attributionLink!.innerText, 'foo');
  });
});
