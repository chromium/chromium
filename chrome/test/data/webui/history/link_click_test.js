// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, listenForPrivilegedLinkClicks} from 'chrome://history/history.js';
import {$} from 'chrome://resources/js/util.m.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';

suite('listenForPrivilegedLinkClicks unit test', function() {
  test('click handler', async () => {
    document.body.innerHTML = '';
    const testService = new TestBrowserService();
    BrowserService.instance_ = testService;

    listenForPrivilegedLinkClicks();
    document.body.innerHTML = `
      <a id="file" href="file:///path/to/file">File</a>
      <a id="chrome" href="about:chrome">Chrome</a>
      <a href="about:blank"><b id="blank">Click me</b></a>
    `;

    $('file').click();
    let clickUrl = await testService.whenCalled('navigateToUrl');
    assertEquals('file:///path/to/file', clickUrl);
    testService.resetResolver('navigateToUrl');

    $('chrome').click();
    clickUrl = await testService.whenCalled('navigateToUrl');
    assertEquals('about:chrome', clickUrl);
    testService.resetResolver('navigateToUrl');

    $('blank').click();
    clickUrl = await testService.whenCalled('navigateToUrl');
    assertEquals('about:blank', clickUrl);
  });
});
