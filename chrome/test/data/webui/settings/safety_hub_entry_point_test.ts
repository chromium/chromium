// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsSafetyHubEntryPointElement} from 'chrome://settings/lazy_load.js';
import {SafetyHubBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, Router, routes, SafetyHubEntryPoint} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

// clang-format on

suite('SafetyHubEntryPoint', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsSafetyHubEntryPointElement;

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-hub-entry-point');
    document.body.appendChild(page);
    await browserProxy.whenCalled('getSafetyHubEntryPointData');
    await microtasksFinished();
  }

  setup(function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
  });

  teardown(function() {
    page.remove();
  });

  test('Safety Hub has recommendations', async function() {
    const header = 'Chrome found some safety recommendations for your review';
    const subheader = 'Passwords, extensions';

    browserProxy.setSafetyHubEntryPointData(
        {'hasRecommendations': true, 'header': header, 'subheader': subheader});
    await createPage();

    assertTrue(page.$.module.hasAttribute('header'));
    assertEquals(page.$.module.getAttribute('header')!.trim(), header);
    assertTrue(page.$.module.hasAttribute('subheader'));
    assertEquals(page.$.module.getAttribute('subheader')!.trim(), subheader);
    assertTrue(page.$.module.hasAttribute('header-icon-color'));
    assertEquals(
        page.$.module.getAttribute('header-icon-color')!.trim(), 'blue');

    // Entry point has primary button leading to Safety Hub.
    assertEquals(page.$.button.getAttribute('class'), 'action-button');
    page.$.button.click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SAFETY_HUB);
  });

  test('Safety Hub has no recommendations', async function() {
    const header = '';
    const subheader = page.i18n('safetyHubEntryPointNothingToDo');

    browserProxy.setSafetyHubEntryPointData({
      'hasRecommendations': false,
      'header': '',
      'subheader': page.i18n('safetyHubEntryPointNothingToDo'),
    });
    await createPage();

    assertEquals(page.$.module.getAttribute('header')!.trim(), header);
    assertTrue(page.$.module.hasAttribute('subheader'));
    assertEquals(page.$.module.getAttribute('subheader')!.trim(), subheader);
    assertTrue(page.$.module.hasAttribute('header-icon-color'));
    assertEquals(page.$.module.getAttribute('header-icon-color')!.trim(), '');

    // Entry point has secondary button leading to Safety Hub.
    assertEquals(page.$.button.getAttribute('class'), '');
    page.$.button.click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SAFETY_HUB);
  });

  test('Impression metric recorded with fetched data', async function() {
    // The entry point is lazily rendered when navigating to the privacy page,
    // so it is attached while the PRIVACY route is already current.
    Router.getInstance().navigateTo(routes.PRIVACY);
    browserProxy.setSafetyHubEntryPointData(
        {'hasRecommendations': true, 'header': 'header', 'subheader': ''});
    await createPage();

    const entryPoint =
        await metricsBrowserProxy.whenCalled('recordSafetyHubEntryPointShown');
    assertEquals(SafetyHubEntryPoint.PRIVACY_WARNING, entryPoint);
    assertEquals(
        1, metricsBrowserProxy.getCallCount('recordSafetyHubEntryPointShown'));
  });

  test('No impression if navigated away before data loads', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-hub-entry-point');
    document.body.appendChild(page);
    // Leave the privacy page before the entry point data is resolved.
    Router.getInstance().navigateTo(routes.BASIC);
    await browserProxy.whenCalled('getSafetyHubEntryPointData');
    await microtasksFinished();

    assertEquals(
        0, metricsBrowserProxy.getCallCount('recordSafetyHubEntryPointShown'));
  });
});
