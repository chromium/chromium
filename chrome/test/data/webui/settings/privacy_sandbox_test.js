// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/privacy_sandbox/app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl} from 'chrome://settings/settings.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {flushTasks} from '../test_util.m.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestOpenWindowProxy} from './test_open_window_proxy.js';

suite('PrivacySandbox', function() {
  /** @type {PrivacySandboxAppElement} */
  let page;

  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {?TestOpenWindowProxy} */
  let openWindowProxy = null;

  /** @type {!TestHatsBrowserProxy} */
  let testHatsBrowserProxy;

  setup(function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.instance_ = testHatsBrowserProxy;

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;

    CrSettingsPrefs.deferInitialization = true;

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.instance_ = openWindowProxy;

    document.body.innerHTML = '';
    page = /** @type {!PrivacySandboxAppElement} */
        (document.createElement('privacy-sandbox-app'));
    document.body.appendChild(page);

    return flushTasks();
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  test('clickApiToggleTest', async function() {
    const toggleButton = page.$$('#apiToggleButton');
    for (const apisEnabledPrior of [true, false]) {
      page.prefs = {
        privacy_sandbox: {
          apis_enabled: {value: apisEnabledPrior},
          manually_controlled: {value: false},
        },
      };
      await flushTasks();
      metricsBrowserProxy.resetResolver('recordAction');
      // User clicks the API toggle.
      toggleButton.click();
      assertTrue(page.prefs.privacy_sandbox.manually_controlled.value);
      // Ensure UMA is logged.
      assertEquals(
          apisEnabledPrior ? 'Settings.PrivacySandbox.ApisDisabled' :
                             'Settings.PrivacySandbox.ApisEnabled',
          await metricsBrowserProxy.whenCalled('recordAction'));
    }
  });

  test('learnMoreTest', async function() {
    // User clicks the "Learn more" button.
    page.$$('#learnMoreButton').click();
    // Ensure UMA is logged.
    assertEquals(
        'Settings.PrivacySandbox.OpenExplainer',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    assertEquals(
        loadTimeData.getString('privacySandboxURL'),
        await openWindowProxy.whenCalled('openURL'));
  });

  test('viewedPref', async function() {
    page.$$('#prefs').initialize();
    await CrSettingsPrefs.initialized;
    assertTrue(!!page.getPref('privacy_sandbox.page_viewed').value);
  });

  test('hatsSurvey', function() {
    // Confirm that the page called out to the HaTS proxy.
    return testHatsBrowserProxy.whenCalled('tryShowPrivacySandboxSurvey');
  });

});
