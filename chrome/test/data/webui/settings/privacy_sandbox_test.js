// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/privacy_sandbox/app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl} from 'chrome://settings/privacy_sandbox/privacy_sandbox_browser_proxy.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';
import {flushTasks, isChildVisible} from '../test_util.m.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestOpenWindowProxy} from './test_open_window_proxy.js';

suite('PrivacySandbox_PrivacySandboxSettings2Disabled', function() {
  /** @type {!PrivacySandboxAppElement} */
  let page;

  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {?TestOpenWindowProxy} */
  let openWindowProxy = null;

  /** @type {!TestHatsBrowserProxy} */
  let testHatsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySandboxSettings2Enabled: false,
    });
  });

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

  test('phase2Visibility', function() {
    assertTrue(isChildVisible(page, '#learnMoreButton'));
    assertTrue(isChildVisible(page, '#pageHeader'));
    assertTrue(isChildVisible(page, '#phase1SettingExplanation'));
    assertFalse(isChildVisible(page, '#flocCard'));
    assertFalse(isChildVisible(page, '#phase2SettingExplanation'));
  });

  test('toggleClass', function() {
    assertEquals('hr', page.$$('#apiToggleButton').className);
  });
});

suite('PrivacySandbox_PrivacySandboxSettings2Enabled', function() {
  /** @type {!PrivacySandboxAppElement} */
  let page;

  /** @type {?TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy = null;

  /**
   * @implements {PrivacySandboxBrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testPrivacySandboxBrowserProxy;

  function setDefaultFlocID() {
    testPrivacySandboxBrowserProxy.setResultFor('getFlocId', Promise.resolve({
      trialStatus: 'test-trial-status',
      cohort: 'test-id',
      nextUpdate: 'test-time',
      canReset: true,
    }));
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySandboxSettings2Enabled: true,
    });
  });

  setup(function() {
    document.body.innerHTML = '';

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;

    testPrivacySandboxBrowserProxy =
        TestBrowserProxy.fromClass(PrivacySandboxBrowserProxy);
    PrivacySandboxBrowserProxyImpl.instance_ = testPrivacySandboxBrowserProxy;

    setDefaultFlocID();

    page = /** @type {!PrivacySandboxAppElement} */
        (document.createElement('privacy-sandbox-app'));
    document.body.appendChild(page);

    page.prefs = {generated: {floc_enabled: {value: true}}};

    return flushTasks();
  });

  test('flocCardVisibility', function() {
    assertTrue(isChildVisible(page, '#flocCard'));
  });

  test('flocId', async function() {
    // The page should automatically retrieve the FLoC state when it is attached
    // to the document.
    await testPrivacySandboxBrowserProxy.whenCalled('getFlocId');
    assertEquals(
        'test-trial-status', page.$$('#flocStatus').textContent.trim());
    assertEquals('test-id', page.$$('#flocId').textContent.trim());
    assertEquals('test-time', page.$$('#flocUpdatedOn').textContent.trim());
    assertFalse(page.$$('#resetFlocIdButton').disabled);

    // The page should listen for changes via a WebUI listener.
    webUIListenerCallback('floc-id-changed', {
      trialStatus: 'new-test-trial-status',
      cohort: 'new-test-id',
      nextUpdate: 'new-test-time',
      canReset: false,
    });

    await flushTasks();
    assertEquals(
        'new-test-trial-status', page.$$('#flocStatus').textContent.trim());
    assertEquals('new-test-id', page.$$('#flocId').textContent.trim());
    assertEquals('new-test-time', page.$$('#flocUpdatedOn').textContent.trim());
    assertTrue(page.$$('#resetFlocIdButton').disabled);
  });

  test('resetFlocId', function() {
    page.$$('#resetFlocIdButton').click();
    return testPrivacySandboxBrowserProxy.whenCalled('resetFlocId');
  });

  test('prefObserver', async function() {
    await testPrivacySandboxBrowserProxy.whenCalled('getFlocId');
    testPrivacySandboxBrowserProxy.resetResolver('getFlocId');

    // When the FLoC generated preference is changed, the page should re-query
    // for the FLoC id.
    setDefaultFlocID();
    page.set('prefs.generated.floc_enabled.value', false);
    await testPrivacySandboxBrowserProxy.whenCalled('getFlocId');
  });

  test('phase2Visibility', function() {
    assertFalse(isChildVisible(page, '#learnMoreButton'));
    assertFalse(isChildVisible(page, '#pageHeader'));
    assertFalse(isChildVisible(page, '#phase1SettingExplanation'));
    assertTrue(isChildVisible(page, '#flocCard'));
    assertTrue(isChildVisible(page, '#phase2SettingExplanation'));
  });

  test('toggleClass', function() {
    assertEquals(
        'hr updated-toggle-button', page.$$('#apiToggleButton').className);
  });

  test('userActions', async function() {
    page.$$('#flocToggleButton').click();
    assertEquals(
        'Settings.PrivacySandbox.FlocDisabled',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');

    page.$$('#flocToggleButton').click();
    assertEquals(
        'Settings.PrivacySandbox.FlocEnabled',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');

    // Ensure that an action is only recorded in response to interaction with
    // the toggle, and not for the generated preference changing.
    page.set('prefs.generated.floc_enabled.value', false);
    await flushTasks();
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));
  });
});
