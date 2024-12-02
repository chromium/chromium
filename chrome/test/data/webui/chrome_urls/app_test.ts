// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-urls/app.js';

import type {ChromeUrlsAppElement} from 'chrome://chrome-urls/app.js';
import {BrowserProxyImpl} from 'chrome://chrome-urls/browser_proxy.js';
import type {WebuiUrlInfo} from 'chrome://chrome-urls/chrome_urls.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestChromeUrlsBrowserProxy} from './test_chrome_urls_browser_proxy.js';

suite('ChromeUrlsAppTest', function() {
  const commandUrls: Url[] =
      [{url: 'chrome://kill/'}, {url: 'chrome://crash/'}];

  let app: ChromeUrlsAppElement;
  let browserProxy: TestChromeUrlsBrowserProxy;

  async function finishSetup(
      webuiUrls: WebuiUrlInfo[], internalDebuggingUisEnabled: boolean = false) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestChromeUrlsBrowserProxy();
    browserProxy.handler.setTestData(
        {webuiUrls, commandUrls, internalDebuggingUisEnabled});
    BrowserProxyImpl.setInstance(browserProxy);
    app = document.createElement('chrome-urls-app');
    document.body.appendChild(app);
    await browserProxy.handler.whenCalled('getUrls');
    await microtasksFinished();
  }

  function assertWebUiItems(webuiItems: NodeListOf<HTMLElement>) {
    assertEquals(2, webuiItems.length);

    // Enabled URLs should be linked.
    const link = webuiItems[0]!.querySelector('a');
    assertTrue(!!link);
    assertEquals('chrome://settings/', link.href);
    assertEquals('chrome://settings', link.textContent);

    // Disabled URLs are not linked, but still display the address.
    const noLink = webuiItems[1]!.querySelector('a');
    assertFalse(!!noLink);
    assertEquals('chrome://bookmarks', webuiItems[1]!.textContent);
  }

  function assertHeadings(internalsSection: boolean) {
    const headings = app.shadowRoot!.querySelectorAll('h2');
    const expectedLength = internalsSection ? 3 : 2;
    assertEquals(expectedLength, headings.length);
    assertEquals('List of Chrome URLs', headings[0]!.textContent);
    assertEquals(
        'Command URLs for Debug', headings[expectedLength - 1]!.textContent);
    if (internalsSection) {
      assertEquals('Internal Debugging Page URLs', headings[1]!.textContent);
    }
  }

  test('Fetches and displays URL list', async () => {
    const webuiUrls: WebuiUrlInfo[] = [
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
    ];
    await finishSetup(webuiUrls);

    const lists = app.shadowRoot!.querySelectorAll('ul');
    assertEquals(2, lists.length);
    const webuiItems = lists[0]!.querySelectorAll('li');
    assertWebUiItems(webuiItems);

    // Command URLs do not have links.
    const commandItems = lists[1]!.querySelectorAll('li');
    assertEquals(2, commandItems.length);
    for (let i = 0; i < commandItems.length; i++) {
      const item = commandItems[i]!;
      assertFalse(!!item.querySelector('a'));
      assertEquals(commandUrls[i]!.url, item.textContent + '/');
    }

    assertHeadings(false);
  });

  test('Correctly displays internal URLs when disabled', async () => {
    const webuiUrls: WebuiUrlInfo[] = [
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
      {url: {url: 'chrome://webui-gallery/'}, enabled: true, internal: true},
    ];
    await finishSetup(webuiUrls);

    const lists = app.shadowRoot!.querySelectorAll('ul');
    assertEquals(3, lists.length);
    const webuiItems = lists[0]!.querySelectorAll('li');
    assertWebUiItems(webuiItems);

    const internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    assertEquals('chrome://webui-gallery', internalItems[0]!.textContent);
    assertFalse(!!internalItems[0]!.querySelector('a'));

    const message = lists[1]!.previousElementSibling;
    assertTrue(!!message);
    const status = message.querySelector('span.bold');
    assertTrue(!!status);
    assertEquals('disabled', status.textContent);

    assertHeadings(true);
  });

  test('Correctly displays internal URLs when enabled', async () => {
    const webuiUrls: WebuiUrlInfo[] = [
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
      {url: {url: 'chrome://webui-gallery/'}, enabled: true, internal: true},
    ];
    await finishSetup(webuiUrls, /*internalDebuggingUisEnabled=*/ true);

    const lists = app.shadowRoot!.querySelectorAll('ul');
    assertEquals(3, lists.length);
    const webuiItems = lists[0]!.querySelectorAll('li');
    assertWebUiItems(webuiItems);

    const internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    const link = internalItems[0]!.querySelector('a');
    assertTrue(!!link);
    assertEquals('chrome://webui-gallery/', link.href);
    assertEquals('chrome://webui-gallery', link.textContent);

    const message = lists[1]!.previousElementSibling;
    assertTrue(!!message);
    const status = message.querySelector('span.bold');
    assertTrue(!!status);
    assertEquals('enabled', status.textContent);

    assertHeadings(true);
  });
});
