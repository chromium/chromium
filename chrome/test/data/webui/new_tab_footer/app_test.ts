// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://newtab-footer/app.js';

import type {NewTabFooterAppElement} from 'chrome://newtab-footer/app.js';
import {NewTabFooterDocumentProxy} from 'chrome://newtab-footer/browser_proxy.js';
import type {ManagementNotice, NewTabFooterDocumentRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabFooterAppTest', () => {
  let element: NewTabFooterAppElement;
  let handler: TestMock<NewTabFooterHandlerRemote>&NewTabFooterHandlerRemote;
  let callbackRouter: NewTabFooterDocumentRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(NewTabFooterHandlerRemote);
    NewTabFooterDocumentProxy.setInstance(
        handler, new NewTabFooterDocumentCallbackRouter());
    callbackRouter = NewTabFooterDocumentProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function initializeElement() {
    element = document.createElement('new-tab-footer-app');
    document.body.appendChild(element);
    await microtasksFinished();
    await handler.whenCalled('updateManagementNotice');
  }

  suite('Extension', () => {
    test('Get extension name on initialization', async () => {
      // Arrange.
      handler.setResultFor('getNtpExtensionName', {name: 'foo'});
      await initializeElement();

      // Assert.
      const name = $$(element, '#extensionName');
      assertTrue(!!name);
      const link = name.querySelector<HTMLElement>('[role="link"]');
      assertTrue(!!link);
      assertEquals(link.innerText, 'foo');
    });

    test('Click extension name link', async () => {
      // Arrange.
      handler.setResultFor('getNtpExtensionName', {name: 'foo'});
      await initializeElement();

      // Act.
      const link = $$(element, '#extensionName [role="link"]');
      assertTrue(!!link);
      link.click();

      // Assert.
      assertEquals(
          1, handler.getCallCount('openExtensionOptionsPageWithFallback'));
    });
  });

  suite('Managed', () => {
    test('Get management notice on initialization', async () => {
      // Arrange.
      await initializeElement();
      const managementNotice:
          ManagementNotice = {text: 'Managed by your organization'};

      // Act.
      callbackRouter.setManagementNotice(managementNotice);
      await callbackRouter.$.flushForTesting();

      // Assert.
      const managementNoticeText = $$(element, '#managementNoticeText');
      assertTrue(!!managementNoticeText);
      assertEquals(
          managementNoticeText.textContent, 'Managed by your organization');
    });
  });
});
