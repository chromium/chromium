// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LocalDataBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Router,routes} from 'chrome://settings/settings.js';
import {TestLocalDataBrowserProxy} from 'chrome://test/settings/test_local_data_browser_proxy.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('SiteDataTest', function() {
  /** @type {SiteDataElement} */
  let siteData;

  /** @type {TestLocalDataBrowserProxy} */
  let testBrowserProxy;

  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  setup(function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    testBrowserProxy = new TestLocalDataBrowserProxy();
    LocalDataBrowserProxyImpl.instance_ = testBrowserProxy;
    siteData = document.createElement('site-data');
    siteData.filter = '';
  });

  teardown(function() {
    siteData.remove();
  });

  test('remove button (trash) calls remove on origin', async function() {
    const sites = [
      {site: 'Hello', localData: 'Cookiez!'},
    ];
    testBrowserProxy.setCookieList(sites);
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);

    await eventToPromise('site-data-list-complete', siteData);
    flush();
    const button = siteData.$$('site-data-entry').$$('.icon-delete-gray');
    assertTrue(!!button);
    assertEquals('CR-ICON-BUTTON', button.tagName);
    button.click();
    const path = await testBrowserProxy.whenCalled('removeSite');
    assertEquals('Hello', path);
    const metric =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.SITE_DATA_REMOVE_SITE, metric);
  });

  test('remove button hidden when no search results', async function() {
    const sites = [
      {site: 'Hello', localData: 'Cookiez!'},
      {site: 'World', localData: 'Cookiez!'},
    ];
    testBrowserProxy.setCookieList(sites);
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);

    await eventToPromise('site-data-list-complete', siteData);
    assertEquals(2, siteData.$.list.items.length);
    siteData.filter = 'Hello';
    await eventToPromise('site-data-list-complete', siteData);
    assertEquals(1, siteData.$.list.items.length);
  });

  test('calls reloadCookies() when created', async function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.COOKIES);
    await testBrowserProxy.whenCalled('reloadCookies');
  });

  test('calls reloadCookies() when visited again', async function() {
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.COOKIES);
    testBrowserProxy.reset();
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    await testBrowserProxy.whenCalled('reloadCookies');
  });

  test('no call to reloadCookies() on same route navigation', async function() {
    // Check that providing a search query parameter while navigating and a
    // search filter to the cookies page does not reload cookies, but instead
    // updates the list.
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    await testBrowserProxy.whenCalled('reloadCookies');

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    await flushTasks();
    assertEquals(1, testBrowserProxy.getCallCount('reloadCookies'));
  });

  test('remove button records interaction metric', async function() {
    // Check that the remove button correctly records an interaction metric
    // based on whether the list is filtered or not.
    document.body.appendChild(siteData);
    siteData.$$('#removeShowingSites').click();
    flush();

    siteData.$$('.action-button').click();
    let metric =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.SITE_DATA_REMOVE_ALL, metric);
    testMetricsBrowserProxy.reset();

    // Add a filter and repeat.
    siteData.filter = 'Test';
    siteData.$$('#removeShowingSites').click();
    flush();

    siteData.$$('.action-button').click();
    metric =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.SITE_DATA_REMOVE_FILTERED, metric);
  });

  test('changes to filter update list', async function() {
    // Check that updating the page filter correctly calls update display list.
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.COOKIES);
    flush();

    // Changes to the filter while not on the site data page should not be
    // responded to.
    siteData.set('filter', 'Test');
    await flushTasks();
    assertEquals(0, testBrowserProxy.getCallCount('getDisplayList'));

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    let filter = await testBrowserProxy.whenCalled('getDisplayList');
    assertEquals('Test', filter);
    testBrowserProxy.reset();

    siteData.set('filter', 'Test2');
    filter = await testBrowserProxy.whenCalled('getDisplayList');
    assertEquals('Test2', filter);
  });

});
