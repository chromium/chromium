// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';

import {ComposeboxProxyImpl, SearchboxBrowserProxy} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import type {OmniboxEverywhereComposeboxElement, OmniboxEverywhereOmniboxElement} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxState} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxEverywhereOmniboxTest', () => {
  let omnibox: OmniboxEverywhereOmniboxElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    omnibox = document.createElement('omnibox-everywhere-omnibox');
    document.body.appendChild(omnibox);
    await microtasksFinished();
  });

  test('onAddTabContext_ opens composebox with tab upload', () => {
    let openComposeboxCalled = false;
    const detailHolder: {state?: ComposeboxState} = {};
    omnibox.addEventListener('open-composebox', (e: Event) => {
      openComposeboxCalled = true;
      detailHolder.state = (e as CustomEvent).detail as ComposeboxState;
    });

    const contextMenu = omnibox.shadowRoot.querySelector('#context')!;
    contextMenu.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {
        id: 123,
        title: 'Test Tab Title',
        url: 'https://example.com' as unknown as Url,
        delayUpload: false,
        origin: TabUploadOrigin.CONTEXT_MENU,
      },
      bubbles: true,
      composed: true,
    }));

    assertTrue(openComposeboxCalled);
    const files = detailHolder.state!.files;
    assertEquals(1, files.length);
    assertEquals(123, (files[0] as {tabId: number}).tabId);
    assertEquals('Test Tab Title', (files[0] as {title: string}).title);
    assertEquals('https://example.com', (files[0] as {url: Url}).url);
    assertEquals(
        TabUploadOrigin.CONTEXT_MENU,
        (files[0] as {origin: TabUploadOrigin}).origin);
  });
});

suite('OmniboxEverywhereComposeboxTest', () => {
  let composebox: OmniboxEverywhereComposeboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let mockPageHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler,
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));

    composebox = document.createElement('omnibox-everywhere-composebox');
    document.body.appendChild(composebox);
    await microtasksFinished();
  });

  test('onAddTabContext adds tab to composebox files', async () => {
    const mockToken = {high: 1234n, low: 5678n};
    testProxy.handler.setPromiseResolveFor('addTabContext', mockToken);

    const contextMenu =
        composebox.shadowRoot.querySelector('#contextEntrypoint')!;
    contextMenu.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {
        id: 789,
        title: 'Composebox Direct Tab',
        url: 'https://direct.com' as unknown as Url,
        delayUpload: false,
        origin: TabUploadOrigin.CONTEXT_MENU,
      },
      bubbles: true,
      composed: true,
    }));

    await testProxy.handler.whenCalled('addTabContext');
    const args = testProxy.handler.getArgs('addTabContext')[0];
    assertEquals(789, args[0]);
    assertFalse(args[1]);

    await microtasksFinished();
    assertEquals(1, composebox.files.size);
    const file = Array.from(composebox.files.values())[0]!;
    assertEquals(789, file.tabId);
    assertEquals('Composebox Direct Tab', file.name);
  });
});
