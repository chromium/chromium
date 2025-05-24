// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://newtab-footer/app.js';

import type {NewTabFooterAppElement} from 'chrome://newtab-footer/app.js';
import {NewTabFooterDocumentProxy} from 'chrome://newtab-footer/browser_proxy.js';
import type {ManagementNotice, NewTabFooterDocumentRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      await initializeElement();

      // Act.
      const fooName = 'foo';
      callbackRouter.setNtpExtensionName(fooName);
      await callbackRouter.$.flushForTesting();

      // Assert.
      let name = $$(element, '#extensionName');
      assertTrue(!!name);
      const link = name.querySelector<HTMLElement>('[role="link"]');
      assertTrue(!!link);
      assertEquals(link.innerText, fooName);

      // Act.
      callbackRouter.setNtpExtensionName('');
      await callbackRouter.$.flushForTesting();

      // Assert.
      name = $$(element, '#extensionName');
      assertFalse(!!name);
    });

    test('Click extension name link', async () => {
      // Arrange.
      callbackRouter.setNtpExtensionName('foo');
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
    test('Get management notice', async () => {
      // Arrange.
      await initializeElement();
      const managementNotice: ManagementNotice = {
        text: 'Managed by your organization',
        bitmapDataUrl: {url: 'chrome://resources/images/chrome_logo_dark.svg'},
      };

      // Act.
      callbackRouter.setManagementNotice(managementNotice);
      await callbackRouter.$.flushForTesting();

      // Assert.
      const managementNoticeContainer =
          element.shadowRoot.querySelector('#managementNoticeContainer');
      assertTrue(!!managementNoticeContainer);
      let managementNoticeText = managementNoticeContainer.querySelector('p');
      assertTrue(!!managementNoticeText);
      assertEquals(
          managementNoticeText.innerText, 'Managed by your organization');
      let managementNoticeLogo =
          managementNoticeContainer.querySelector<HTMLImageElement>('img');
      assertTrue(!!managementNoticeLogo);
      assertEquals(
          managementNoticeLogo.src,
          'chrome://resources/images/chrome_logo_dark.svg');

      // Act.
      callbackRouter.setManagementNotice(null);
      await callbackRouter.$.flushForTesting();

      // Assert.
      managementNoticeText = $$(element, '#managementNoticeContainer p');
      managementNoticeLogo = $$(element, '#managementNoticeLogo');
      assertFalse(!!managementNoticeText);
      assertFalse(!!managementNoticeLogo);
    });
  });
});
