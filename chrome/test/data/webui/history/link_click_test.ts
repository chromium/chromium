// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl, getTrustedHTML} from 'chrome://history/history.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';

const NO_MODIFIERS = {
  middleButton: false,
  altKey: false,
  ctrlKey: false,
  metaKey: false,
  shiftKey: false,
};

suite('listenForPrivilegedLinkClicks unit test', function() {
  test('click handler', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);

    document.body.innerHTML = getTrustedHTML`
      <a id="file" href="file:///path/to/file">File</a>
      <a id="chrome" href="about:chrome" target="_blank">Chrome</a>
      <a href="about:blank"><b id="blank">Click me</b></a>
    `;

    let defaultClickEventPromise = eventToPromise('click', document);
    getRequiredElement('file').click();
    await defaultClickEventPromise;
    assertEquals(0, testProxy.handler.getCallCount('navigateToUrl'));

    // Add history-app to add listeners.
    const appElement = document.createElement('history-app');
    document.body.appendChild(appElement);

    getRequiredElement('file').click();
    let [clickUrl, openInNewTab, modifiers] =
        await testProxy.handler.whenCalled('navigateToUrl');
    assertEquals('file:///path/to/file', clickUrl);
    assertFalse(openInNewTab);
    assertDeepEquals(NO_MODIFIERS, modifiers);
    assertEquals(1, testProxy.handler.getCallCount('navigateToUrl'));
    testProxy.handler.resetResolver('navigateToUrl');

    getRequiredElement('chrome').click();
    [clickUrl, openInNewTab, modifiers] =
        await testProxy.handler.whenCalled('navigateToUrl');
    assertEquals('about:chrome', clickUrl);
    assertTrue(openInNewTab);
    assertDeepEquals(NO_MODIFIERS, modifiers);
    testProxy.handler.resetResolver('navigateToUrl');

    getRequiredElement('blank').click();
    [clickUrl, openInNewTab, modifiers] =
        await testProxy.handler.whenCalled('navigateToUrl');
    assertEquals('about:blank', clickUrl);
    assertFalse(openInNewTab);
    assertDeepEquals(NO_MODIFIERS, modifiers);
    testProxy.handler.resetResolver('navigateToUrl');

    // Middle-click with keyboard modifiers.
    getRequiredElement('file').dispatchEvent(new MouseEvent('auxclick', {
      button: 1,
      bubbles: true,
      cancelable: true,
      ctrlKey: true,
      shiftKey: true,
    }));
    [clickUrl, openInNewTab, modifiers] =
        await testProxy.handler.whenCalled('navigateToUrl');
    assertEquals('file:///path/to/file', clickUrl);
    assertFalse(openInNewTab);
    assertDeepEquals(
        {
          middleButton: true,
          altKey: false,
          ctrlKey: true,
          metaKey: false,
          shiftKey: true,
        },
        modifiers);
    testProxy.handler.resetResolver('navigateToUrl');

    // Removing the element should remove listeners.
    appElement.remove();
    defaultClickEventPromise = eventToPromise('click', document);
    getRequiredElement('blank').click();
    await defaultClickEventPromise;
    assertEquals(0, testProxy.handler.getCallCount('navigateToUrl'));
  });
});
