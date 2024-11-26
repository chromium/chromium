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
  const webuiUrls: WebuiUrlInfo[] = [
    {url: {url: 'chrome://settings'}, enabled: true},
    {url: {url: 'chrome://bookmarks'}, enabled: false},
  ];

  const commandUrls: Url[] = [{url: 'chrome://kill'}, {url: 'chrome://crash'}];

  let app: ChromeUrlsAppElement;
  let browserProxy: TestChromeUrlsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestChromeUrlsBrowserProxy();
    browserProxy.handler.setTestData({webuiUrls, commandUrls});
    BrowserProxyImpl.setInstance(browserProxy);
    app = document.createElement('chrome-urls-app');
    document.body.appendChild(app);
  });

  test('Fetches and displays URL list', async () => {
    await browserProxy.handler.whenCalled('getUrls');
    await microtasksFinished();

    const lists = app.shadowRoot!.querySelectorAll('ul');
    assertEquals(2, lists.length);
    const webuiItems = lists[0]!.querySelectorAll('li');
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

    // Command URLs do not have links.
    const commandItems = lists[1]!.querySelectorAll('li');
    assertEquals(2, commandItems.length);
    for (let i = 0; i < commandItems.length; i++) {
      const item = commandItems[i]!;
      assertFalse(!!item.querySelector('a'));
      assertEquals(commandUrls[i]!.url, item.textContent);
    }

    // Validate headings
    const headings = app.shadowRoot!.querySelectorAll('h1');
    assertEquals(2, headings.length);
    assertEquals('List of Chrome URLs', headings[0]!.textContent);
    assertEquals('For Debug', headings[1]!.textContent);
  });
});
