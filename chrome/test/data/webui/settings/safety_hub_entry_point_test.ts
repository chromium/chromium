// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {EntryPointInfo, SettingsSafetyHubEntryPointElement} from 'chrome://settings/lazy_load.js';
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

  test(
      'Refreshes data and impression metric when returning to privacy route',
      async function() {
        const warningHeader =
            'Chrome found some safety recommendations for your review';
        const warningSubheader = 'Safe Browsing';
        const safeSubheader = 'No recommendations right now';

        Router.getInstance().navigateTo(routes.PRIVACY);
        browserProxy.setSafetyHubEntryPointData({
          'hasRecommendations': true,
          'header': warningHeader,
          'subheader': warningSubheader,
        });
        await createPage();

        // Verify baseline warning state and initial impression metric.
        assertEquals(
            warningHeader, page.$.module.getAttribute('header')!.trim());
        assertEquals(
            warningSubheader, page.$.module.getAttribute('subheader')!.trim());
        assertEquals(
            'blue', page.$.module.getAttribute('header-icon-color')!.trim());
        assertEquals('action-button', page.$.button.getAttribute('class'));
        assertEquals(
            SafetyHubEntryPoint.PRIVACY_WARNING,
            await metricsBrowserProxy.whenCalled(
                'recordSafetyHubEntryPointShown'));
        assertEquals(
            1,
            metricsBrowserProxy.getCallCount('recordSafetyHubEntryPointShown'));

        // Navigate to Safety Check, resolve recommendations, and navigate back
        // to the Privacy page while the element remains connected.
        page.$.button.click();
        assertEquals(routes.SAFETY_HUB, Router.getInstance().getCurrentRoute());

        browserProxy.resetResolver('getSafetyHubEntryPointData');
        metricsBrowserProxy.resetResolver('recordSafetyHubEntryPointShown');
        browserProxy.setSafetyHubEntryPointData({
          'hasRecommendations': false,
          'header': '',
          'subheader': safeSubheader,
        });

        Router.getInstance().navigateTo(routes.PRIVACY);
        await browserProxy.whenCalled('getSafetyHubEntryPointData');
        await microtasksFinished();

        assertEquals('', page.$.module.getAttribute('header')!.trim());
        assertEquals(
            safeSubheader, page.$.module.getAttribute('subheader')!.trim());
        assertEquals(
            '', page.$.module.getAttribute('header-icon-color')!.trim());
        assertEquals('', page.$.button.getAttribute('class'));
        assertEquals(
            SafetyHubEntryPoint.PRIVACY_SAFE,
            await metricsBrowserProxy.whenCalled(
                'recordSafetyHubEntryPointShown'));
        assertEquals(
            1,
            metricsBrowserProxy.getCallCount('recordSafetyHubEntryPointShown'));
      });

  test(
      'Ignores stale out-of-order entry point responses across route changes',
      async function() {
        const firstRequest = new PromiseResolver<EntryPointInfo>();
        const secondRequest = new PromiseResolver<EntryPointInfo>();
        let callIndex = 0;
        browserProxy.getSafetyHubEntryPointData = () => {
          browserProxy.methodCalled('getSafetyHubEntryPointData');
          callIndex++;
          return callIndex === 1 ? firstRequest.promise : secondRequest.promise;
        };

        Router.getInstance().navigateTo(routes.PRIVACY);
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        page = document.createElement('settings-safety-hub-entry-point');
        document.body.appendChild(page);
        await browserProxy.whenCalled('getSafetyHubEntryPointData');

        // Navigate away and back to PRIVACY before the first request resolves.
        Router.getInstance().navigateTo(routes.SAFETY_HUB);
        browserProxy.resetResolver('getSafetyHubEntryPointData');
        Router.getInstance().navigateTo(routes.PRIVACY);
        await browserProxy.whenCalled('getSafetyHubEntryPointData');

        // Resolve the newer request first (safe state), then the stale first
        // request (warning state).
        secondRequest.resolve({
          'hasRecommendations': false,
          'header': '',
          'subheader': 'No recommendations right now',
        });
        await microtasksFinished();

        firstRequest.resolve({
          'hasRecommendations': true,
          'header': 'Stale warning header',
          'subheader': 'Stale subheader',
        });
        await microtasksFinished();

        assertEquals('', page.$.module.getAttribute('header')!.trim());
        assertEquals(
            'No recommendations right now',
            page.$.module.getAttribute('subheader')!.trim());
        assertEquals('', page.$.button.getAttribute('class'));
        assertEquals(
            SafetyHubEntryPoint.PRIVACY_SAFE,
            await metricsBrowserProxy.whenCalled(
                'recordSafetyHubEntryPointShown'));
        assertEquals(
            1,
            metricsBrowserProxy.getCallCount('recordSafetyHubEntryPointShown'));
      });
});
