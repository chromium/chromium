// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/privacy_sandbox/app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {PrivacySandboxAppElement} from 'chrome://settings/privacy_sandbox/app.js';
import {PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl} from 'chrome://settings/privacy_sandbox/privacy_sandbox_browser_proxy.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, MetricsBrowserProxyImpl, TrustSafetyInteraction} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

class TestPrivacySandboxBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxBrowserProxy {
  constructor() {
    super(['getFlocId', 'resetFlocId']);
  }

  getFlocId() {
    this.methodCalled('getFlocId');
    return Promise.resolve({
      trialStatus: 'test-trial-status',
      cohort: 'test-id',
      nextUpdate: 'test-time',
      canReset: true,
    });
  }

  resetFlocId() {
    this.methodCalled('resetFlocId');
  }
}

suite('PrivacySandbox', function() {
  let page: PrivacySandboxAppElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;

  setup(function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);

    CrSettingsPrefs.deferInitialization = true;

    document.body.innerHTML = '';
    page = /** @type {!PrivacySandboxAppElement} */
        (document.createElement('privacy-sandbox-app'));
    document.body.appendChild(page);

    page.prefs = {generated: {floc_enabled: {value: true}}};

    return flushTasks();
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  test('clickApiToggleTest', async function() {
    const toggleButton =
        page.shadowRoot!.querySelector<HTMLElement>('#apiToggleButton')!;
    for (const apisEnabledPrior of [true, false]) {
      page.prefs = {
        privacy_sandbox: {
          apis_enabled: {value: apisEnabledPrior},
          manually_controlled: {value: false},
        },
        generated: {floc_enabled: {value: true}}
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

  test('viewedPref', async function() {
    page.shadowRoot!.querySelector('settings-prefs')!.initialize();
    await CrSettingsPrefs.initialized;
    assertTrue(!!page.getPref('privacy_sandbox.page_viewed').value);
  });

  test('hatsSurvey', async function() {
    // Confirm that the page called out to the HaTS proxy.
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_PRIVACY_SANDBOX, interaction);
  });

  test('flocId', async function() {
    // The page should automatically retrieve the FLoC state when it is attached
    // to the document.
    await testPrivacySandboxBrowserProxy.whenCalled('getFlocId');
    assertEquals('test-trial-status', page.$.flocStatus.textContent!.trim());
    assertEquals('test-id', page.$.flocId.textContent!.trim());
    assertEquals('test-time', page.$.flocUpdatedOn!.textContent!.trim());
    assertFalse(page.$.resetFlocIdButton.disabled);

    // The page should listen for changes via a WebUI listener.
    webUIListenerCallback('floc-id-changed', {
      trialStatus: 'new-test-trial-status',
      cohort: 'new-test-id',
      nextUpdate: 'new-test-time',
      canReset: false,
    });

    await flushTasks();
    assertEquals(
        'new-test-trial-status', page.$.flocStatus.textContent!.trim());
    assertEquals('new-test-id', page.$.flocId.textContent!.trim());
    assertEquals('new-test-time', page.$.flocUpdatedOn!.textContent!.trim());
    assertTrue(page.$.resetFlocIdButton.disabled);
  });

  test('resetFlocId', function() {
    page.$.resetFlocIdButton.click();
    return testPrivacySandboxBrowserProxy.whenCalled('resetFlocId');
  });

  test('prefObserver', async function() {
    await testPrivacySandboxBrowserProxy.whenCalled('getFlocId');
    testPrivacySandboxBrowserProxy.resetResolver('getFlocId');

    // When the FLoC generated preference is changed, the page should re-query
    // for the FLoC id.
    testPrivacySandboxBrowserProxy.resetResolver('getFlocId');
    page.set('prefs.generated.floc_enabled.value', false);
    await testPrivacySandboxBrowserProxy.whenCalled('getFlocId');
  });

  test('userActions', async function() {
    page.$.flocToggleButton.click();
    assertEquals(
        'Settings.PrivacySandbox.FlocDisabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    page.$.flocToggleButton.click();
    assertEquals(
        'Settings.PrivacySandbox.FlocEnabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Ensure that an action is only recorded in response to interaction with
    // the toggle, and not for the generated preference changing.
    page.set('prefs.generated.floc_enabled.value', false);
    await flushTasks();
    assertEquals(0, metricsBrowserProxy.getCallCount('recordAction'));
  });
});
