// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserServiceImpl, getTrustedHTML, listenForPrivilegedLinkClicks} from 'chrome://history/history.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestBrowserService} from './test_browser_service.js';

suite('listenForPrivilegedLinkClicks unit test', function() {
  test('click handler', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    listenForPrivilegedLinkClicks();
    document.body.innerHTML = getTrustedHTML`
      <a id="file" href="file:///path/to/file">File</a>
      <a id="chrome" href="about:chrome">Chrome</a>
      <a href="about:blank"><b id="blank">Click me</b></a>
    `;

    getRequiredElement('file').click();
    let clickUrl = await testService.whenCalled('navigateToUrl');
    assertEquals('file:///path/to/file', clickUrl);
    testService.resetResolver('navigateToUrl');

    getRequiredElement('chrome').click();
    clickUrl = await testService.whenCalled('navigateToUrl');
    assertEquals('about:chrome', clickUrl);
    testService.resetResolver('navigateToUrl');

    getRequiredElement('blank').click();
    clickUrl = await testService.whenCalled('navigateToUrl');
    assertEquals('about:blank', clickUrl);
  });
});
