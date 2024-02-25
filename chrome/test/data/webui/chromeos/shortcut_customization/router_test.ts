// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {RouteObserver, Router} from 'chrome://shortcut-customization/js/router.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('RouterTest', function() {
  class FakeRouteObserver implements RouteObserver {
    lastUrl?: URL;
    numCalls: number;

    constructor() {
      this.numCalls = 0;
    }

    onRouteChanged(url: URL): void {
      this.numCalls++;
      this.lastUrl = url;
    }
  }

  test('Basic router test', async () => {
    Router.resetInstanceForTesting(new Router());
    const url = new URL('chrome://shortcut-customization');
    url.searchParams.append('testParam', 'testValue');
    Router.getInstance().navigateTo(url);
    assertEquals(
        'chrome://shortcut-customization/?testParam=testValue',
        window.location.href);
  });

  test('Reset route test', async () => {
    Router.resetInstanceForTesting(new Router());
    const url = new URL('chrome://shortcut-customization');
    url.searchParams.append('testParam', 'testValue');
    Router.getInstance().navigateTo(url);
    assertEquals(
        'chrome://shortcut-customization/?testParam=testValue',
        window.location.href);
    Router.getInstance().resetRoute();
    assertEquals('chrome://shortcut-customization/', window.location.href);
  });

  test('Observer test', async () => {
    Router.resetInstanceForTesting(new Router());
    const router = Router.getInstance();

    // Add an observer, and verify that it gets notified.
    const observer = new FakeRouteObserver();
    router.addObserver(observer);

    let url = new URL('chrome://shortcut-customization');
    url.searchParams.append('testParam', 'testValue');

    assertEquals(0, observer.numCalls);
    assertEquals(undefined, observer.lastUrl);
    router.navigateTo(url);
    assertEquals(1, observer.numCalls);
    assertEquals(url, observer.lastUrl);

    // Remove the observer, and verify that it doesn't get notified.
    router.removeObserver(observer);

    const lastUrl = url;
    url = new URL('chrome://shortcut-customization');
    url.searchParams.append('otherParam', 'otherValue');

    assertEquals(1, observer.numCalls);
    assertEquals(lastUrl, observer.lastUrl);
    router.navigateTo(url);
    assertEquals(1, observer.numCalls);
    assertEquals(lastUrl, observer.lastUrl);
    assertNotEquals(url, observer.lastUrl);
  });
});
