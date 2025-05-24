// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-urls/app.js';

import type {ChromeUrlsAppElement} from 'chrome://chrome-urls/app.js';
import {INTERNAL_DEBUG_PAGES_HASH} from 'chrome://chrome-urls/app.js';
import {BrowserProxyImpl} from 'chrome://chrome-urls/browser_proxy.js';
import type {WebuiUrlInfo} from 'chrome://chrome-urls/chrome_urls.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    assertEquals(3, webuiItems.length);

    // Enabled URLs should be linked.
    // Special case for chrome://chrome-urls, see crbug.com/411626175
    const chromeUrlsLink = webuiItems[0]!.querySelector('a');
    assertTrue(!!chromeUrlsLink);
    const location = window.location.href;
    assertEquals(`${location}#`, chromeUrlsLink.href);
    assertEquals('chrome://chrome-urls', chromeUrlsLink.textContent);

    const link = webuiItems[1]!.querySelector('a');
    assertTrue(!!link);
    assertEquals('chrome://settings/', link.href);
    assertEquals('chrome://settings', link.textContent);

    // Disabled URLs are not linked, but still display the address.
    const noLink = webuiItems[2]!.querySelector('a');
    assertFalse(!!noLink);
    assertEquals('chrome://bookmarks', webuiItems[2]!.textContent);
  }

  function assertHeadings(internalsSection: boolean) {
    const headings = app.shadowRoot.querySelectorAll('h2');
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
      {url: {url: 'chrome://chrome-urls/'}, enabled: true, internal: false},
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
    ];
    await finishSetup(webuiUrls);

    const lists = app.shadowRoot.querySelectorAll('ul');
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
      {url: {url: 'chrome://chrome-urls/'}, enabled: true, internal: false},
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
      {url: {url: 'chrome://webui-gallery/'}, enabled: true, internal: true},
    ];
    await finishSetup(webuiUrls);

    const lists = app.shadowRoot.querySelectorAll('ul');
    assertEquals(3, lists.length);
    const webuiItems = lists[0]!.querySelectorAll('li');
    assertWebUiItems(webuiItems);

    const internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    assertEquals('chrome://webui-gallery', internalItems[0]!.textContent);
    assertFalse(!!internalItems[0]!.querySelector('a'));

    const message = app.shadowRoot.querySelector('#debug-pages-description');
    assertTrue(!!message);
    const status = message.querySelector('span.bold');
    assertTrue(!!status);
    assertEquals('disabled', status.textContent);

    assertHeadings(true);
  });

  test('Correctly displays internal URLs when enabled', async () => {
    const webuiUrls: WebuiUrlInfo[] = [
      {url: {url: 'chrome://chrome-urls/'}, enabled: true, internal: false},
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
      {url: {url: 'chrome://webui-gallery/'}, enabled: true, internal: true},
    ];
    await finishSetup(webuiUrls, /*internalDebuggingUisEnabled=*/ true);

    const lists = app.shadowRoot.querySelectorAll('ul');
    assertEquals(3, lists.length);
    const webuiItems = lists[0]!.querySelectorAll('li');
    assertWebUiItems(webuiItems);

    const internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    const link = internalItems[0]!.querySelector('a');
    assertTrue(!!link);
    assertEquals('chrome://webui-gallery/', link.href);
    assertEquals('chrome://webui-gallery', link.textContent);

    const message = app.shadowRoot.querySelector('#debug-pages-description');
    assertTrue(!!message);
    const status = message.querySelector('span.bold');
    assertTrue(!!status);
    assertEquals('enabled', status.textContent);

    assertHeadings(true);
  });

  test('Toggle debug UIs enabled', async () => {
    const webuiUrls: WebuiUrlInfo[] = [
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
      {url: {url: 'chrome://webui-gallery/'}, enabled: true, internal: true},
    ];
    await finishSetup(webuiUrls);

    const lists = app.shadowRoot.querySelectorAll('ul');
    assertEquals(3, lists.length);

    // No links since debug pages are disabled.
    let internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    assertEquals('chrome://webui-gallery', internalItems[0]!.textContent);
    assertFalse(!!internalItems[0]!.querySelector('a'));

    // Message is set to 'disabled' and button is to enable the pages.
    const message = app.shadowRoot.querySelector('#debug-pages-description');
    assertTrue(!!message);
    const status = message.querySelector('span.bold');
    assertTrue(!!status);
    assertEquals('disabled', status.textContent);
    const button = app.shadowRoot.querySelector('cr-button');
    assertTrue(!!button);
    assertEquals('Enable internal debugging pages', button.textContent!.trim());

    // Test case of enabling debug pages.
    button.click();
    let enabled = await browserProxy.handler.whenCalled('setDebugPagesEnabled');
    assertTrue(enabled);
    await microtasksFinished();
    // Status is enabled, button is to disable, and page is linked.
    assertEquals('enabled', status.textContent);
    assertEquals(
        'Disable internal debugging pages', button.textContent!.trim());
    internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    assertTrue(!!internalItems[0]!.querySelector('a'));

    // Test case of disabling debug pages.
    browserProxy.handler.resetResolver('setDebugPagesEnabled');
    button.click();
    enabled = await browserProxy.handler.whenCalled('setDebugPagesEnabled');
    assertFalse(enabled);
    await microtasksFinished();
    assertEquals('disabled', status.textContent);
    assertEquals('Enable internal debugging pages', button.textContent!.trim());
    internalItems = lists[1]!.querySelectorAll('li');
    assertEquals(1, internalItems.length);
    assertFalse(!!internalItems[0]!.querySelector('a'));
  });

  test('Navigate to debug UI headings', async () => {
    // Ensure we need to scroll by making a short window and lots of urls.
    // We do this by making the <body> scrollable and document fixed height
    // since we can't resize the window itself in tests.
    document.documentElement.style.height = '200px';
    document.documentElement.style.maxHeight = '200px';
    document.documentElement.style.overflow = 'hidden';
    document.body.style.height = '100%';
    document.body.style.overflow = 'auto';

    window.history.replaceState({}, '', `/#${INTERNAL_DEBUG_PAGES_HASH}`);
    window.dispatchEvent(new CustomEvent('popstate'));
    const webuiUrls: WebuiUrlInfo[] = [
      {url: {url: 'chrome://settings/'}, enabled: true, internal: false},
      {url: {url: 'chrome://extensions/'}, enabled: true, internal: false},
      {url: {url: 'chrome://downloads/'}, enabled: true, internal: false},
      {url: {url: 'chrome://print/'}, enabled: true, internal: false},
      {url: {url: 'chrome://history/'}, enabled: true, internal: false},
      {url: {url: 'chrome://new-tab-page/'}, enabled: true, internal: false},
      {url: {url: 'chrome://whats-new/'}, enabled: true, internal: false},
      {url: {url: 'chrome://bookmarks/'}, enabled: false, internal: false},
      {url: {url: 'chrome://test-1/'}, enabled: false, internal: false},
      {url: {url: 'chrome://test-2/'}, enabled: false, internal: false},
      {url: {url: 'chrome://test-3/'}, enabled: false, internal: false},
      {url: {url: 'chrome://test-4/'}, enabled: false, internal: false},
      {url: {url: 'chrome://test-5/'}, enabled: false, internal: false},
      {url: {url: 'chrome://webui-gallery/'}, enabled: true, internal: true},
    ];
    await finishSetup(webuiUrls);

    const header =
        app.shadowRoot.querySelector<HTMLElement>('#internal-debugging-pages');
    assertTrue(!!header);
    // Header should be in the viewport.
    assertGT(
        200 + document.body.scrollTop,
        app.offsetTop + header.offsetTop + header.offsetHeight);
  });
});
