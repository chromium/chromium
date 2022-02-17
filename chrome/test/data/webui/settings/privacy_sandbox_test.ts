// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/privacy_sandbox/app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement} from 'chrome://settings/lazy_load.js';
import {PrivacySandboxAppElement, PrivacySandboxSettingsView} from 'chrome://settings/privacy_sandbox/app.js';
import {CanonicalTopic, PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl} from 'chrome://settings/privacy_sandbox/privacy_sandbox_browser_proxy.js';
import {CrButtonElement, CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, TrustSafetyInteraction} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

class TestPrivacySandboxBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxBrowserProxy {
  constructor() {
    super([
      'getFlocId', 'resetFlocId', 'getFledgeState', 'setFledgeJoiningAllowed'
    ]);
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

  getFledgeState() {
    this.methodCalled('getFledgeState');
    return Promise.resolve({
      joiningSites: ['test-site-one.com'],
      blockedSites: ['test-site-two.com'],
    });
  }

  setFledgeJoiningAllowed(site: string, allowed: boolean) {
    this.methodCalled('setFledgeJoiningAllowed', [site, allowed]);
  }

  getTopicsState() {
    this.methodCalled('getTopicsState');
    return Promise.resolve({
      topTopics:
          [{topicId: 1, taxonomyVersion: 1, displayString: 'test-topic-1'}],
      blockedTopics:
          [{topicId: 2, taxonomyVersion: 1, displayString: 'test-topic-2'}],
    });
  }

  setTopicAllowed(topic: CanonicalTopic, allowed: boolean) {
    this.methodCalled('setTopicAllowed', [topic, allowed]);
  }
}

suite('PrivacySandbox', function() {
  let page: PrivacySandboxAppElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySandboxSettings3Enabled: false,
    });
  });

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

  test('testSandboxSettings3Visibility', function() {
    assertTrue(isChildVisible(page, '#trialsCard'));
    assertTrue(isChildVisible(page, '#flocCard'));
    assertFalse(isChildVisible(page, '#trialsCardSettings3'));
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
    assertEquals(
        'test-trial-status',
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#flocStatus')!.textContent!.trim());
    assertEquals(
        'test-id',
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#flocId')!.textContent!.trim());
    assertEquals(
        'test-time',
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#flocUpdatedOn')!.textContent!.trim());
    assertFalse(
        page.shadowRoot!.querySelector<CrButtonElement>(
                            '#resetFlocIdButton')!.disabled);

    // The page should listen for changes via a WebUI listener.
    webUIListenerCallback('floc-id-changed', {
      trialStatus: 'new-test-trial-status',
      cohort: 'new-test-id',
      nextUpdate: 'new-test-time',
      canReset: false,
    });

    await flushTasks();
    assertEquals(
        'new-test-trial-status',
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#flocStatus')!.textContent!.trim());
    assertEquals(
        'new-test-id',
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#flocId')!.textContent!.trim());
    assertEquals(
        'new-test-time',
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#flocUpdatedOn')!.textContent!.trim());
    assertTrue(
        page.shadowRoot!.querySelector<CrButtonElement>(
                            '#resetFlocIdButton')!.disabled);
  });

  test('resetFlocId', function() {
    page.shadowRoot!.querySelector<HTMLElement>('#resetFlocIdButton')!.click();
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
    page.shadowRoot!.querySelector<HTMLElement>('#flocToggleButton')!.click();
    assertEquals(
        'Settings.PrivacySandbox.FlocDisabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    page.shadowRoot!.querySelector<HTMLElement>('#flocToggleButton')!.click();
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

suite('PrivacySandboxSettings3', function() {
  let page: PrivacySandboxAppElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySandboxSettings3Enabled: true,
    });
  });

  setup(function() {
    assertTrue(loadTimeData.getBoolean('privacySandboxSettings3Enabled'));
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = '';
    page = /** @type {!PrivacySandboxAppElement} */
        (document.createElement('privacy-sandbox-app'));
    document.body.appendChild(page);

    return flushTasks();
  });

  function assertMainViewVisible() {
    assertEquals(
        page.privacySandboxSettingsView_, PrivacySandboxSettingsView.MAIN);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertFalse(!!dialogWrapper);
  }

  function assertLearnMoreDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView_,
        PrivacySandboxSettingsView.LEARN_MORE_DIALOG);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertTrue(!!dialogWrapper);
    assertTrue(dialogWrapper.open);
    assertTrue(
        page.shadowRoot!
            .querySelector<DomIf>(
                '#' + PrivacySandboxSettingsView.LEARN_MORE_DIALOG)!.if !);
  }

  function assertAdPersonalizationDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView_,
        PrivacySandboxSettingsView.AD_PERSONALIZATION_DIALOG);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertTrue(!!dialogWrapper);
    assertTrue(dialogWrapper.open);
    assertTrue(
        page.shadowRoot!
            .querySelector<DomIf>(
                '#' +
                PrivacySandboxSettingsView.AD_PERSONALIZATION_DIALOG)!.if !);
  }

  function assertAdMeasurementDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView_,
        PrivacySandboxSettingsView.AD_MEASUREMENT_DIALOG);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertTrue(!!dialogWrapper);
    assertTrue(dialogWrapper.open);
    assertTrue(
        page.shadowRoot!
            .querySelector<DomIf>(
                '#' + PrivacySandboxSettingsView.AD_MEASUREMENT_DIALOG)!.if !);
  }

  function assertSpamAndFraudDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView_,
        PrivacySandboxSettingsView.SPAM_AND_FRAUD_DIALOG);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertTrue(!!dialogWrapper);
    assertTrue(dialogWrapper.open);
    assertTrue(
        page.shadowRoot!
            .querySelector<DomIf>(
                '#' + PrivacySandboxSettingsView.SPAM_AND_FRAUD_DIALOG)!.if !);
  }

  test('testSandboxSettings3Visibility', function() {
    assertFalse(isChildVisible(page, '#trialsCard'));
    assertFalse(isChildVisible(page, '#flocCard'));
    assertTrue(isChildVisible(page, '#trialsCardSettings3'));
  });

  [true, false].forEach(apisEnabledPrior => {
    test(`clickTrialsToggleTest_${apisEnabledPrior}`, async () => {
      const trialsToggle =
          page.shadowRoot!.querySelector<HTMLElement>('#trialsToggle')!;
      page.prefs = {
        privacy_sandbox: {
          apis_enabled_v2: {value: apisEnabledPrior},
          manually_controlled: {value: false},
        },
      };
      await flushTasks();
      metricsBrowserProxy.resetResolver('recordAction');
      // User clicks the trials toggle.
      trialsToggle.click();
      assertEquals(
          !apisEnabledPrior,
          page.getPref('privacy_sandbox.apis_enabled_v2').value);
      assertTrue(page.prefs.privacy_sandbox.manually_controlled.value);
      // Ensure UMA is logged.
      assertEquals(
          apisEnabledPrior ? 'Settings.PrivacySandbox.ApisDisabled' :
                             'Settings.PrivacySandbox.ApisEnabled',
          await metricsBrowserProxy.whenCalled('recordAction'));
    });
  });

  test('testLearnMoreDialog', async function() {
    // The learn more link should be visible but the dialog itself not.
    assertMainViewVisible();
    assertTrue(isChildVisible(page, '#learnMoreLink'));

    // Clicking on the learn more link should open the dialog.
    page.shadowRoot!.querySelector<HTMLElement>('#learnMoreLink')!.click();
    await flushTasks();
    assertLearnMoreDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testAdPersonalizationDialog', async function() {
    assertMainViewVisible();

    // Clicking on the ad personalization row should open the dialog.
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#adPersonalizationRow')!.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testAdMeasurementDialog', async function() {
    assertMainViewVisible();

    // Clicking on the ad measurement row should open the dialog.
    page.shadowRoot!.querySelector<HTMLElement>('#adMeasurementRow')!.click();
    await flushTasks();
    assertAdMeasurementDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testAdMeasurementDialogBrowsingHistoryLink', async function() {
    assertMainViewVisible();
    page.shadowRoot!.querySelector<HTMLElement>('#adMeasurementRow')!.click();
    await flushTasks();
    assertAdMeasurementDialogVisible();

    // Check that the browsing history link exists and goes to the right place.
    const controlMeasurementDescription =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#adMeasurementDialogControlMeasurement')!;
    const browsingHistoryLink =
        controlMeasurementDescription.querySelector<HTMLAnchorElement>(
            'a[href]');
    assertTrue(!!browsingHistoryLink);
    assertEquals('chrome://history/', browsingHistoryLink.href);
  });

  test('testSpamAndFraudDialog', async function() {
    assertMainViewVisible();

    // Clicking on the spam & fraud row should open the dialog.
    page.shadowRoot!.querySelector<HTMLElement>('#spamAndFraudRow')!.click();
    await flushTasks();
    assertSpamAndFraudDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });
});
