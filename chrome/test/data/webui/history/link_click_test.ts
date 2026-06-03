// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl, getTrustedHTML} from 'chrome://history/history.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';

suite('listenForPrivilegedLinkClicks unit test', function() {
  test('click handler', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);

    document.body.innerHTML = getTrustedHTML`
      <a id="file" href="file:///path/to/file">File</a>
      <a id="chrome" href="about:chrome">Chrome</a>
      <a href="about:blank"><b id="blank">Click me</b></a>
    `;

    let defaultClickEventPromise = eventToPromise('click', document);
    getRequiredElement('file').click();
    await defaultClickEventPromise;
    assertEquals(0, testProxy.getCallCount('navigateToUrl'));

    // Add history-app to add listeners.
    const appElement = document.createElement('history-app');
    document.body.appendChild(appElement);

    getRequiredElement('file').click();
    let clickUrl = await testProxy.whenCalled('navigateToUrl');
    assertEquals('file:///path/to/file', clickUrl);
    assertEquals(1, testProxy.getCallCount('navigateToUrl'));
    testProxy.resetResolver('navigateToUrl');

    getRequiredElement('chrome').click();
    clickUrl = await testProxy.whenCalled('navigateToUrl');
    assertEquals('about:chrome', clickUrl);
    testProxy.resetResolver('navigateToUrl');

    getRequiredElement('blank').click();
    clickUrl = await testProxy.whenCalled('navigateToUrl');
    assertEquals('about:blank', clickUrl);
    testProxy.resetResolver('navigateToUrl');

    // Removing the element should remove listeners.
    appElement.remove();
    defaultClickEventPromise = eventToPromise('click', document);
    getRequiredElement('blank').click();
    await defaultClickEventPromise;
    assertEquals(0, testProxy.getCallCount('navigateToUrl'));
  });
});
