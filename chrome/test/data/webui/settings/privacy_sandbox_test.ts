// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/privacy_sandbox/app.js';

import {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement} from 'chrome://settings/lazy_load.js';
import {PrivacySandboxAppElement, PrivacySandboxSettingsView} from 'chrome://settings/privacy_sandbox/app.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacySandboxBrowserProxyImpl, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacySandboxBrowserProxy} from './test_privacy_sandbox_browser_proxy.js';

suite('PrivacySandboxSettings', function() {
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

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = /** @type {!PrivacySandboxAppElement} */
        (document.createElement('privacy-sandbox-app'));
    document.body.appendChild(page);
    page.prefs = {privacy_sandbox: {apis_enabled_v2: {value: true}}};
    return flushTasks();
  });

  function assertMainViewVisible() {
    assertEquals(
        page.privacySandboxSettingsView, PrivacySandboxSettingsView.MAIN);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertFalse(!!dialogWrapper);
  }

  function assertLearnMoreDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView,
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
        page.privacySandboxSettingsView,
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
    const removedRow = page.shadowRoot!.querySelector<HTMLElement>(
        '.ad-personalization-removed-row')!;
    assertTrue(isVisible(removedRow));
    const backButton = page.shadowRoot!.querySelector<HTMLElement>(
        '#adPersonalizationBackButton')!;
    assertFalse(isVisible(backButton));
  }

  function assertAdPersonalizationRemovedDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView,
        PrivacySandboxSettingsView.AD_PERSONALIZATION_REMOVED_DIALOG);
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertTrue(!!dialogWrapper);
    assertTrue(dialogWrapper.open);
    assertTrue(page.shadowRoot!
                   .querySelector<DomIf>(
                       '#' +
                       PrivacySandboxSettingsView
                           .AD_PERSONALIZATION_REMOVED_DIALOG)!.if !);
    const removedRow = page.shadowRoot!.querySelector<HTMLElement>(
        '.ad-personalization-removed-row')!;
    assertFalse(isVisible(removedRow));
    const backButton = page.shadowRoot!.querySelector<HTMLElement>(
        '#adPersonalizationBackButton')!;
    assertTrue(isVisible(backButton));
  }

  function assertAdMeasurementDialogVisible() {
    assertEquals(
        page.privacySandboxSettingsView,
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
        page.privacySandboxSettingsView,
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

  [true, false].forEach(apisEnabledPrior => {
    test(`clickTrialsToggleTest_${apisEnabledPrior}`, async () => {
      const trialsToggle =
          page.shadowRoot!.querySelector<HTMLElement>('#trialsToggle')!;
      page.prefs = {
        privacy_sandbox: {
          apis_enabled_v2: {value: apisEnabledPrior},
          manually_controlled_v2: {value: false},
        },
      };
      await flushTasks();
      metricsBrowserProxy.resetResolver('recordAction');
      // User clicks the trials toggle.
      trialsToggle.click();
      assertEquals(
          !apisEnabledPrior,
          page.getPref('privacy_sandbox.apis_enabled_v2').value);
      assertTrue(page.prefs.privacy_sandbox.manually_controlled_v2.value);
      // Ensure UMA is logged.
      assertEquals(
          apisEnabledPrior ? 'Settings.PrivacySandbox.ApisDisabled' :
                             'Settings.PrivacySandbox.ApisEnabled',
          await metricsBrowserProxy.whenCalled('recordAction'));

      if (apisEnabledPrior) {
        // Check that top topics & joining sites have been cleared.
        assertMainViewVisible();
        page.$.adPersonalizationRow.click();
        await flushTasks();
        assertAdPersonalizationDialogVisible();

        const topTopicsSection =
            page.shadowRoot!.querySelector<HTMLElement>('#topTopicsSection')!;
        const topTopics = topTopicsSection.querySelector('dom-repeat');
        assertTrue(!!topTopics);
        assertEquals(0, topTopics.items!.length);
        assertTrue(
            isVisible(topTopicsSection.querySelector('#topTopicsEmpty')));

        const joiningSitesSection = page.shadowRoot!.querySelector<HTMLElement>(
            '#joiningSitesSection')!;
        const joiningSites = joiningSitesSection.querySelector('dom-repeat');
        assertTrue(!!joiningSites);
        assertEquals(0, joiningSites.items!.length);
        assertTrue(
            isVisible(joiningSitesSection.querySelector('#joiningSitesEmpty')));
      }
    });
  });

  test('testLearnMoreDialog', async function() {
    // The learn more link should be visible but the dialog itself not.
    assertMainViewVisible();
    assertTrue(isVisible(page.$.learnMoreLink));

    // Clicking on the learn more link should open the dialog.
    page.$.learnMoreLink.click();
    assertEquals(
        'Settings.PrivacySandbox.AdPersonalization.LearnMoreClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
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
    page.$.adPersonalizationRow.click();
    assertEquals(
        'Settings.PrivacySandbox.AdPersonalization.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    await flushTasks();
    assertAdPersonalizationDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testAdPersonalizationDialogRemovedPage', async function() {
    assertMainViewVisible();

    // Clicking on the ad personalization row should open the dialog.
    page.$.adPersonalizationRow.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();
    metricsBrowserProxy.resetResolver('recordAction');

    // Clicking on the link row for removed interests should take you to the
    // removed interests page.
    page.shadowRoot!
        .querySelector<HTMLElement>('.ad-personalization-removed-row')!.click();
    assertEquals(
        'Settings.PrivacySandbox.RemovedInterests.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    await flushTasks();
    assertAdPersonalizationRemovedDialogVisible();

    // Clicking on the back button should take you back to the main ad
    // personalization dialog.
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#adPersonalizationBackButton')!.click();
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
    page.$.adMeasurementRow.click();
    assertEquals(
        'Settings.PrivacySandbox.AdMeasurement.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    await flushTasks();
    assertAdMeasurementDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testAdMeasurementDialogBrowsingHistoryLink', async function() {
    assertMainViewVisible();
    page.$.adMeasurementRow.click();
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
    page.$.spamAndFraudRow.click();
    assertEquals(
        'Settings.PrivacySandbox.SpamFraud.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    await flushTasks();
    assertSpamAndFraudDialogVisible();

    // Clicking on the close button of the dialog should close it.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testTopicsList', async function() {
    assertMainViewVisible();
    page.$.adPersonalizationRow.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();

    // Assert topic visible on main page.
    const topTopicsSection =
        page.shadowRoot!.querySelector<HTMLElement>('#topTopicsSection')!;
    const topTopics = topTopicsSection.querySelector('dom-repeat');
    assertTrue(!!topTopics);
    assertEquals(1, topTopics.items!.length);
    assertFalse(isVisible(topTopicsSection.querySelector('#topTopicsEmpty')));
    assertEquals('test-topic-1', topTopics.items![0].topic!.displayString);

    // Switch to removed page.
    page.shadowRoot!
        .querySelector<HTMLElement>('.ad-personalization-removed-row')!.click();
    await flushTasks();
    assertAdPersonalizationRemovedDialogVisible();

    // Assert topic on removed page.
    const blockedTopicsSection =
        page.shadowRoot!.querySelector('#blockedTopicsSection')!;
    let blockedTopics = blockedTopicsSection.querySelector('dom-repeat');
    assertTrue(!!blockedTopics);
    assertEquals(1, blockedTopics.items!.length);
    assertFalse(
        isVisible(blockedTopicsSection.querySelector('#blockedTopicsEmpty')));
    assertEquals('test-topic-2', blockedTopics.items![0].topic!.displayString);

    // Switch to main page.
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#adPersonalizationBackButton')!.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();
    metricsBrowserProxy.resetResolver('recordAction');

    // Remove topic from main page.
    const item =
        topTopicsSection.querySelector('privacy-sandbox-interest-item')!;
    item.shadowRoot!.querySelector('cr-button')!.click();
    assertEquals(
        'Settings.PrivacySandbox.AdPersonalization.TopicRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Assert the topic is no longer visible.
    assertEquals(
        0, topTopicsSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(topTopicsSection.querySelector('#topTopicsEmpty')));

    // Switch to removed page.
    page.shadowRoot!
        .querySelector<HTMLElement>('.ad-personalization-removed-row')!.click();
    await flushTasks();
    assertAdPersonalizationRemovedDialogVisible();
    metricsBrowserProxy.resetResolver('recordAction');

    // Assert the topic was moved to removed page.
    blockedTopics = blockedTopicsSection.querySelector('dom-repeat')!;
    assertEquals(2, blockedTopics.items!.length);
    assertEquals('test-topic-1', blockedTopics.items![0].topic!.displayString);
    assertEquals('test-topic-2', blockedTopics.items![1].topic!.displayString);

    // Unblock topics from removed page.
    const items =
        blockedTopicsSection.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, items.length);
    for (const item of items) {
      item.shadowRoot!.querySelector('cr-button')!.click();
      assertEquals(
          'Settings.PrivacySandbox.RemovedInterests.TopicAdded',
          await metricsBrowserProxy.whenCalled('recordAction'));
    }

    // Assert all topics are gone.
    assertEquals(
        0, blockedTopicsSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(
        isVisible(blockedTopicsSection.querySelector('#blockedTopicsEmpty')));

    // Close dialog.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('testFledgeList', async function() {
    assertMainViewVisible();
    page.$.adPersonalizationRow.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();

    // Assert site visible on main page.
    const joiningSitesSection =
        page.shadowRoot!.querySelector<HTMLElement>('#joiningSitesSection')!;
    const joiningSites = joiningSitesSection.querySelector('dom-repeat');
    assertTrue(!!joiningSites);
    assertEquals(1, joiningSites.items!.length);
    assertFalse(
        isVisible(joiningSitesSection.querySelector('#joiningSitesEmpty')));
    assertEquals('test-site-one.com', joiningSites.items![0].site!);

    // Switch to removed page.
    page.shadowRoot!
        .querySelector<HTMLElement>('.ad-personalization-removed-row')!.click();
    await flushTasks();
    assertAdPersonalizationRemovedDialogVisible();

    // Assert site on removed page.
    const blockedSitesSection =
        page.shadowRoot!.querySelector('#blockedSitesSection')!;
    let blockedSites = blockedSitesSection.querySelector('dom-repeat');
    assertTrue(!!blockedSites);
    assertEquals(1, blockedSites.items!.length);
    assertFalse(
        isVisible(blockedSitesSection.querySelector('#blockedSitesEmpty')));
    assertEquals('test-site-two.com', blockedSites.items![0].site!);

    // Switch to main page.
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#adPersonalizationBackButton')!.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();
    metricsBrowserProxy.resetResolver('recordAction');

    // Remove site from main page.
    const item =
        joiningSitesSection.querySelector('privacy-sandbox-interest-item')!;
    item.shadowRoot!.querySelector('cr-button')!.click();
    assertEquals(
        'Settings.PrivacySandbox.AdPersonalization.SiteRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Assert the site is no longer visible.
    assertEquals(
        0, joiningSitesSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(
        isVisible(joiningSitesSection.querySelector('#joiningSitesEmpty')));

    // Switch to removed page.
    page.shadowRoot!
        .querySelector<HTMLElement>('.ad-personalization-removed-row')!.click();
    await flushTasks();
    assertAdPersonalizationRemovedDialogVisible();
    metricsBrowserProxy.resetResolver('recordAction');

    // Assert the site was moved to removed page.
    blockedSites = blockedSitesSection.querySelector('dom-repeat')!;
    assertEquals(2, blockedSites.items!.length);
    assertEquals('test-site-one.com', blockedSites.items![0].site!);
    assertEquals('test-site-two.com', blockedSites.items![1].site!);

    // Unblock sites from removed page.
    const items =
        blockedSitesSection.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, items.length);
    for (const item of items) {
      item.shadowRoot!.querySelector('cr-button')!.click();
      assertEquals(
          'Settings.PrivacySandbox.RemovedInterests.SiteAdded',
          await metricsBrowserProxy.whenCalled('recordAction'));
    }

    // Assert all sites are gone.
    assertEquals(
        0, blockedSitesSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(
        isVisible(blockedSitesSection.querySelector('#blockedSitesEmpty')));

    // Close dialog.
    page.shadowRoot!.querySelector<HTMLElement>('#dialogCloseButton')!.click();
    await flushTasks();
    assertMainViewVisible();
  });

  test('directDialogClose', async function() {
    // Confirm that closing the dialog directly (as done through the escape key)
    // correctly navigates back.
    assertMainViewVisible();

    // Open a sub page.
    page.$.adPersonalizationRow.click();
    await flushTasks();
    assertAdPersonalizationDialogVisible();

    // Close the subpage by closing the modal dialog directly.
    const dialogWrapper =
        page.shadowRoot!.querySelector<CrDialogElement>('#dialogWrapper');
    assertTrue(!!dialogWrapper);
    dialogWrapper.close();
    // The close() call on the the <cr-dialog> is routed to its internal
    // <dialog> element, which then fires close, which the <cr-dialog> then
    // fires it's own close event from. Not all of these tasks are necessarily
    // queued synchronously. Waiting until the <cr-dialog> fires close, and
    // then an additional flushTasks() so the page can react, is required.
    await eventToPromise('close', dialogWrapper);
    await flushTasks();

    assertMainViewVisible();

    // Closing the dialog should have reset the view state, such that another
    // dialog can be opened.
    page.$.adMeasurementRow.click();
    await flushTasks();
    assertAdMeasurementDialogVisible();
  });
});
