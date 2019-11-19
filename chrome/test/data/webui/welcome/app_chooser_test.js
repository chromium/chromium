// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://welcome/google_apps/nux_google_apps.js';

import {GoogleAppProxyImpl} from 'chrome://welcome/google_apps/google_app_proxy.js';
import {GoogleAppsMetricsProxyImpl} from 'chrome://welcome/google_apps/google_apps_metrics_proxy.js';
import {BookmarkBarManager, BookmarkProxyImpl} from 'chrome://welcome/shared/bookmark_proxy.js';

import {TestBookmarkProxy} from './test_bookmark_proxy.js';
import {TestGoogleAppProxy} from './test_google_app_proxy.js';
import {TestMetricsProxy} from './test_metrics_proxy.js';

suite('AppChooserTest', function() {
  const apps = [
    {
      id: 0,
      name: 'First',
      icon: 'first',
      url: 'http://first.example.com',
    },
    {
      id: 1,
      name: 'Second',
      icon: 'second',
      url: 'http://second.example.com',
    },
    {
      id: 2,
      name: 'Third',
      icon: 'third',
      url: 'http://third.example.com',
    },
    {
      id: 3,
      name: 'Fourth',
      icon: 'fourth',
      url: 'http://fourth.example.com',
    },
    {
      id: 4,
      name: 'Fifth',
      icon: 'fifth',
      url: 'http://fifth.example.com',
    },
  ];

  /** @type {NuxAppProxy} */
  let testAppBrowserProxy;

  /** @type {ModuleMetricsProxy} */
  let testAppMetricsProxy;

  /** @type {BookmarkProxy} */
  let testBookmarkBrowserProxy;

  /** @type {AppChooserElement} */
  let testElement;

  setup(async function() {
    testAppBrowserProxy = new TestGoogleAppProxy();
    testAppMetricsProxy = new TestMetricsProxy();
    testBookmarkBrowserProxy = new TestBookmarkProxy();

    GoogleAppProxyImpl.instance_ = testAppBrowserProxy;
    GoogleAppsMetricsProxyImpl.instance_ = testAppMetricsProxy;
    BookmarkProxyImpl.instance_ = testBookmarkBrowserProxy;
    BookmarkBarManager.instance_ = new BookmarkBarManager();

    testAppBrowserProxy.setAppList(apps);

    PolymerTest.clearBody();

    testElement = document.createElement('nux-google-apps');
    document.body.appendChild(testElement);

    // Simulate nux-app's onRouteEnter call.
    testElement.onRouteEnter();
    await testAppMetricsProxy.whenCalled('recordPageShown');
    await testAppBrowserProxy.whenCalled('getAppList');
  });

  teardown(function() {
    testElement.remove();
  });

  function getSelected() {
    return Array.from(
        testElement.shadowRoot.querySelectorAll('.option[active]'));
  }

  test('test app chooser options', async function() {
    const options =
        Array.from(testElement.shadowRoot.querySelectorAll('.option'));
    assertEquals(5, options.length);

    // First three options are selected and action button should be enabled.
    assertDeepEquals(options.slice(0, 3), getSelected());
    assertFalse(testElement.$$('.action-button').disabled);

    // Click the first option to deselect it.
    testBookmarkBrowserProxy.reset();
    options[0].click();

    assertEquals(
        1, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
    assertDeepEquals(options.slice(1, 3), getSelected());
    assertFalse(testElement.$$('.action-button').disabled);

    // Click fourth option to select it.
    testBookmarkBrowserProxy.reset();
    options[3].click();

    assertDeepEquals(
        {
          title: apps[3].name,
          url: apps[3].url,
          parentId: '1',
        },
        await testBookmarkBrowserProxy.whenCalled('addBookmark'));

    assertDeepEquals(options.slice(1, 4), getSelected());
    assertFalse(testElement.$$('.action-button').disabled);

    // Click fourth option again to deselect it.
    testBookmarkBrowserProxy.reset();
    options[3].click();

    assertEquals(
        4, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
    assertDeepEquals(options.slice(1, 3), getSelected());
    assertFalse(testElement.$$('.action-button').disabled);

    // Click second option to deselect it.
    testBookmarkBrowserProxy.reset();
    options[1].click();

    assertEquals(
        2, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
    assertDeepEquals(options.slice(2, 3), getSelected());
    assertFalse(testElement.$$('.action-button').disabled);

    // Click third option to deselect all options.
    testBookmarkBrowserProxy.reset();
    options[2].click();

    assertEquals(
        3, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
    assertEquals(0, getSelected().length);
    assertTrue(testElement.$$('.action-button').disabled);
  });

  test('test app chooser skip button', async function() {
    const options = testElement.shadowRoot.querySelectorAll('.option');
    testElement.wasBookmarkBarShownOnInit_ = true;

    // First option should be selected and action button should be enabled.
    testElement.$.noThanksButton.click();
    assertEquals(
        1, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
    assertEquals(
        true, await testBookmarkBrowserProxy.whenCalled('toggleBookmarkBar'));
    await testAppMetricsProxy.whenCalled('recordDidNothingAndChoseSkip');
  });

  test('test app chooser next button', async function() {
    const options = testElement.shadowRoot.querySelectorAll('.option');
    testElement.wasBookmarkBarShownOnInit_ = true;

    // First option should be selected and action button should be enabled.
    testElement.$$('.action-button').click();

    await testAppMetricsProxy.whenCalled('recordDidNothingAndChoseNext');

    // Test framework only records first result, but should be called 3 times.
    assertEquals(
        0, await testAppBrowserProxy.whenCalled('recordProviderSelected'));
    assertEquals(3, testAppBrowserProxy.providerSelectedCount);
  });
});
