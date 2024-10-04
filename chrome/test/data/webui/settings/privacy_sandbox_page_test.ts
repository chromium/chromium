// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {CrExpandButtonElement, SettingsPrivacySandboxAdMeasurementSubpageElement, SettingsPrivacySandboxManageTopicsSubpageElement, SettingsPrivacySandboxPageElement, SettingsPrivacySandboxTopicsSubpageElement, SettingsSimpleConfirmationDialogElement} from 'chrome://settings/lazy_load.js';
import {SettingsPrivacySandboxFledgeSubpageElement} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement, CrLinkRowElement, FirstLevelTopicsState, SettingsPrefsElement, TopicsState} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacySandboxBrowserProxyImpl, Router, routes, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isChildVisible, isVisible, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacySandboxBrowserProxy} from './test_privacy_sandbox_browser_proxy.js';

suite('PrivacySandboxPage', function() {
  let page: SettingsPrivacySandboxPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let hatsBrowserProxy: TestHatsBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    hatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(hatsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-page');
    page.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX);
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('privacySandboxLinkRowsVisible', function() {
    assertTrue(isChildVisible(page, '#privacySandboxTopicsLinkRow'));
    assertTrue(isChildVisible(page, '#privacySandboxFledgeLinkRow'));
    assertTrue(isChildVisible(page, '#privacySandboxAdMeasurementLinkRow'));
  });

  test('hatsSurveyRequested', async function() {
    const result =
        await hatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_AD_PRIVACY, result);
  });

  test('privacySandboxTopicsRowSublabel', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    const topicsRow = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#privacySandboxTopicsLinkRow');
    assertTrue(!!topicsRow);
    assertTrue(isVisible(topicsRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageTopicsLinkRowSubLabelEnabled'),
        topicsRow.subLabel);

    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    await flushTasks();
    assertTrue(isVisible(topicsRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageTopicsLinkRowSubLabelDisabled'),
        topicsRow.subLabel);
  });

  test('privacySandboxFledgeRowSublabel', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    const fledgeRow = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#privacySandboxFledgeLinkRow');
    assertTrue(!!fledgeRow);
    assertTrue(isVisible(fledgeRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageFledgeLinkRowSubLabelEnabled'),
        fledgeRow.subLabel);

    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', false);
    await flushTasks();
    assertTrue(isVisible(fledgeRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageFledgeLinkRowSubLabelDisabled'),
        fledgeRow.subLabel);
  });

  test('privacySandboxAdMeasurementRowSublabel', async function() {
    page.setPrefValue('privacy_sandbox.m1.ad_measurement_enabled', true);
    await flushTasks();
    const measurementRow = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#privacySandboxAdMeasurementLinkRow');
    assertTrue(!!measurementRow);
    assertTrue(isVisible(measurementRow));
    assertEquals(
        loadTimeData.getString(
            'adPrivacyPageAdMeasurementLinkRowSubLabelEnabled'),
        measurementRow.subLabel);

    page.setPrefValue('privacy_sandbox.m1.ad_measurement_enabled', false);
    await flushTasks();
    assertTrue(isChildVisible(page, '#privacySandboxAdMeasurementLinkRow'));
    assertEquals(
        loadTimeData.getString(
            'adPrivacyPageAdMeasurementLinkRowSubLabelDisabled'),
        measurementRow.subLabel);
  });

  test('clickPrivacySandboxTopicsLinkRow', async function() {
    const topicsRow = page.shadowRoot!.querySelector<HTMLElement>(
        '#privacySandboxTopicsLinkRow');
    assertTrue(!!topicsRow);
    topicsRow.click();
    assertEquals(
        'Settings.PrivacySandbox.Topics.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertEquals(
        routes.PRIVACY_SANDBOX_TOPICS, Router.getInstance().getCurrentRoute());
  });

  test('clickPrivacySandboxFledgeLinkRow', async function() {
    const fledgeRow = page.shadowRoot!.querySelector<HTMLElement>(
        '#privacySandboxFledgeLinkRow');
    assertTrue(!!fledgeRow);
    fledgeRow.click();
    assertEquals(
        'Settings.PrivacySandbox.Fledge.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertEquals(
        routes.PRIVACY_SANDBOX_FLEDGE, Router.getInstance().getCurrentRoute());
  });

  test('clickPrivacySandboxAdMeasurementLinkRow', async function() {
    const measurementRow = page.shadowRoot!.querySelector<HTMLElement>(
        '#privacySandboxAdMeasurementLinkRow');
    assertTrue(!!measurementRow);
    measurementRow.click();
    assertEquals(
        'Settings.PrivacySandbox.AdMeasurement.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertEquals(
        routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
        Router.getInstance().getCurrentRoute());
  });
});

suite('RestrictedEnabled', function() {
  let page: SettingsPrivacySandboxPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  // When the privacy sandbox is restricted, ensure only measurement is shown.
  test('privacySandboxLinkRowsNotVisible', function() {
    assertFalse(isChildVisible(page, '#privacySandboxTopicsLinkRow'));
    assertFalse(isChildVisible(page, '#privacySandboxFledgeLinkRow'));
    assertTrue(isChildVisible(page, '#privacySandboxAdMeasurementLinkRow'));
  });
});

suite('FledgeSubpage', function() {
  let page: SettingsPrivacySandboxFledgeSubpageElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    testPrivacySandboxBrowserProxy.setFledgeState({
      joiningSites: ['test-site-one.com'],
      blockedSites: ['test-site-two.com'],
    });
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-fledge-subpage');
    page.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_FLEDGE);
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getFledgeState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('secondDescription', async function() {
    const secondDescription =
        page.shadowRoot!.querySelector<HTMLElement>('#secondDescription');
    assert(secondDescription);
    assertEquals(
        secondDescription?.innerText, page.i18n('fledgePageExplanation'));
  });

  test('footerLinks', async function() {
    assertTrue(isChildVisible(page, '#footerV2'));
    const links = page.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
        '#footerV2 a[href]');
    assertEquals(links.length, 3, 'footer should contains three links');
    links.forEach(
        link => assertEquals(
            link.getAttribute('aria-description'),
            loadTimeData.getString('opensInNewTab'),
            'the link should indicate that it will be opened in a new tab'));
    const hrefs = Array.from<HTMLAnchorElement>(links).map(link => link.href);
    const expectedLinks = [
      'chrome://settings/adPrivacy/interests',
      'chrome://settings/cookies',
      'https://support.google.com/chrome?p=ad_privacy',
    ];
    assertDeepEquals(hrefs, expectedLinks);
  });
});


suite('TopicsSubpage', function() {
  let page: SettingsPrivacySandboxTopicsSubpageElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let hatsBrowserProxy: TestHatsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    testPrivacySandboxBrowserProxy.setTestTopicState(getTestTopicsState());
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    hatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(hatsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-topics-subpage');
    page.prefs = settingsPrefs.prefs!;
    page.set('prefs.privacy_sandbox.m1.topics_enabled', {value: true});
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_TOPICS);
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getTopicsState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  function getTestTopicsState(): TopicsState {
    return {
      topTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
        {
          topicId: 3,
          taxonomyVersion: 1,
          displayString: 'test-topic-3',
          description: '',
        },
        {
          topicId: 4,
          taxonomyVersion: 1,
          displayString: 'test-topic-4',
          description: '',
        },
      ],
      blockedTopics: [{
        topicId: 2,
        taxonomyVersion: 1,
        displayString: 'test-topic-2',
        description: '',
      }],
    };
  }

  test('hatsSurveyRequested', async function() {
    const result =
        await hatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_TOPICS_SUBPAGE, result);
  });

  // When prefs.privacy_sandbox.m1.topics_enabled value is false
  // and Proactive Topic Blocking feature is turned on,
  // everything on the ad topics page but the settings toggle button,
  // disclaimer and footer is hidden.
  test('enableTopicsToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertFalse(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabelV2'),
        page.$.topicsToggle.subLabel);
    // Assert V2 Layout for ids to be hidden.
    const idsToBeHidden = [
      '#currentTopicsSection',
      '#currentTopicsHeading',
      '#currentTopicsDescription',
      '#currentTopicsDescriptionEmpty',
      '#currentTopicsDescriptionEmptyTextHeading',
      '#currentTopicsDescriptionEmptyTextV2',
      '#currentTopicsDescriptionDisabled',
      '#blockedTopicsRow',
      '#blockedTopicsDescriptionV2',
      '#blockedTopicsDescriptionEmptyTextHeading',
      '#blockedTopicsDescriptionEmptyTextV2',
      '#blockedTopicsList',
      '#manageTopicsSection',
    ];
    idsToBeHidden.forEach(id => assertFalse(isChildVisible(page, id)));
    // FooterV2 should be visible if pref is on or not.
    assertTrue(isChildVisible(page, '#footerV2'));
    assertEquals(
        0, testPrivacySandboxBrowserProxy.getCallCount('topicsToggleChanged'));

    page.$.topicsToggle.click();
    await flushTasks();
    assertTrue((await testPrivacySandboxBrowserProxy.whenCalled(
        'topicsToggleChanged'))[0]);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Enabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabelV2'),
        page.$.topicsToggle.subLabel);
    assertTrue(!!page.getPref('privacy_sandbox.m1.topics_enabled.value'));
    // Non V2 empty text should not be visible
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));

    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    const blockedTopicsRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow');
    blockedTopicsRow!.click();
    assertEquals(
        'Settings.PrivacySandbox.Topics.BlockedTopicsOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    // Non V2 blocked topics description should not be visible
    assertFalse(isChildVisible(page, '#blockedTopicsDescription'));
    // The blocked topic list is NOT empty after re-enabling the toggle
    assertFalse(
        isChildVisible(page, '#blockedTopicsDescriptionEmptyTextHeading'));
    assertFalse(isChildVisible(page, '#blockedTopicsDescriptionEmptyTextV2'));
    // Assert V2 Layout for ids to be shown.
    const idsToBeShown = [
      '#currentTopicsSection',
      '#currentTopicsHeading',
      '#currentTopicsDescription',
      '#currentTopicsDescriptionEmptyTextHeading',
      '#currentTopicsDescriptionEmptyTextV2',
      '#blockedTopicsRow',
      '#blockedTopicsDescriptionV2',
      '#blockedTopicsList',
      '#manageTopicsSection',
      '#footerV2',
    ];
    idsToBeShown.forEach(id => assertTrue(isChildVisible(page, id)));
  });

  test('disableTopicsToggle', async function() {
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabelV2'),
        page.$.topicsToggle.subLabel);
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertFalse(
        isChildVisible(page, '#currentTopicsDescriptionEmptyTextHeading'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmptyTextV2'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    const blockedTopicsRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow');
    blockedTopicsRow!.click();
    assertEquals(
        'Settings.PrivacySandbox.Topics.BlockedTopicsOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    // Non V2 blocked topics description should not be visible
    assertFalse(isChildVisible(page, '#blockedTopicsDescription'));
    // Blocked topics list is not empty
    assertFalse(
        isChildVisible(page, '#blockedTopicsDescriptionEmptyTextHeading'));
    assertFalse(isChildVisible(page, '#blockedTopicsDescriptionEmptyTextV2'));
    // Assert V2 Layout for ids to be shown.
    const idsToBeShown = [
      '#currentTopicsSection',
      '#currentTopicsHeading',
      '#currentTopicsDescription',
      '#blockedTopicsRow',
      '#blockedTopicsDescriptionV2',
      '#blockedTopicsList',
      '#manageTopicsSection',
      '#footerV2',
    ];
    idsToBeShown.forEach(id => assertTrue(isChildVisible(page, id)));

    assertEquals(
        0, testPrivacySandboxBrowserProxy.getCallCount('topicsToggleChanged'));

    page.$.topicsToggle.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.Disabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertFalse((await testPrivacySandboxBrowserProxy.whenCalled(
        'topicsToggleChanged'))[0]);
    assertTrue(isVisible(page.$.topicsToggle));
    assertFalse(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabelV2'),
        page.$.topicsToggle.subLabel);
    // Assert V2 Layout for ids to be hidden.
    const idsToBeHidden = [
      '#currentTopicsSection',
      '#currentTopicsHeading',
      '#currentTopicsDescription',
      '#currentTopicsDescriptionEmpty',
      '#currentTopicsDescriptionEmptyTextHeading',
      '#currentTopicsDescriptionEmptyTextV2',
      '#currentTopicsDescriptionDisabled',
      '#blockedTopicsRow',
      '#blockedTopicsDescription',
      '#blockedTopicsDescriptionV2',
      '#blockedTopicsDescriptionEmptyTextHeading',
      '#blockedTopicsDescriptionEmptyTextV2',
      '#blockedTopicsList',
      '#manageTopicsSection',
    ];
    idsToBeHidden.forEach(id => assertFalse(isChildVisible(page, id)));
  });

  test('disclaimerLinks', async function() {
    const disclaimer = page.shadowRoot!.querySelector('#disclaimer');
    assertTrue(!!disclaimer);
    assertTrue(isVisible(disclaimer));

    const links = page.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
        '#disclaimer a[href]');

    assertEquals(1, links.length);
    assertEquals(
        links[0]!.getAttribute('aria-description'),
        loadTimeData.getString('opensInNewTab'),
        'the link should indicate that it will be opened in a new tab');

    assertEquals(
        links[0]!.href, 'https://support.google.com/chrome?p=ad_privacy');
  });

  function assertToastOpened() {
    const toast = page.shadowRoot!.querySelector('cr-toast');
    assert(toast);
    assertTrue(toast.open);
    const toastBody =
        page.shadowRoot!.querySelector<HTMLElement>('#unblockTopicToastBody');
    assertEquals(toastBody?.innerText, page.i18n('unblockTopicToastBody'));
    const toastButton =
        page.shadowRoot!.querySelector<CrButtonElement>('#closeToastButton');
    assertEquals(
        toastButton?.innerText, page.i18n('unblockTopicToastButtonText'));
    toastButton?.click();
    assertFalse(toast.open);
  }

  test('blockAndAllowTopics', async function() {
    testPrivacySandboxBrowserProxy.setChildTopics([{
      topicId: 3,
      taxonomyVersion: 1,
      displayString: 'test-topic-3',
      description: '',
    }]);
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    // Check for current topics.
    const currentTopicsSection =
        page.shadowRoot!.querySelector<HTMLElement>('#currentTopicsSection')!;
    const currentTopics =
        currentTopicsSection.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(3, currentTopics.length);
    assertFalse(isVisible(currentTopicsSection.querySelector(
        '#currentTopicsDescriptionEmptyTextHeading')));
    assertFalse(isVisible(currentTopicsSection.querySelector(
        '#currentTopicsDescriptionEmptyTextV2')));
    assert(!!currentTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-1',
        currentTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);
    assert(!!currentTopics[1]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-3',
        currentTopics[1]!.shadowRoot!.querySelector('#label')!.textContent);
    assert(!!currentTopics[2]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-4',
        currentTopics[2]!.shadowRoot!.querySelector('#label')!.textContent);

    // Check for blocked topics.
    const blockedTopicsRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow');
    blockedTopicsRow!.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.BlockedTopicsOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    const blockedTopicsList =
        page.shadowRoot!.querySelector('#blockedTopicsList')!;
    let blockedTopics =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    const blockedTopicsDescription =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#blockedTopicsDescriptionV2')!;
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescriptionNew'),
        blockedTopicsDescription.innerText);
    assertEquals(1, blockedTopics.length);
    assert(!!blockedTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-2',
        blockedTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);

    // Block topic.
    const items =
        currentTopicsSection.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(3, items.length);
    let blockButton = items[0]?.shadowRoot!.querySelector('cr-button');
    assertEquals(
        page.i18n('topicsPageBlockTopicA11yLabel', 'test-topic-1'),
        blockButton!.getAttribute('aria-label'));
    blockButton!.click();
    await flushTasks();
    let blockTopicDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#blockTopicDialog');
    assertTrue(!!blockTopicDialog);
    await (whenAttributeIs(blockTopicDialog.$.dialog, 'open', ''));
    blockTopicDialog.$.cancel.click();
    await eventToPromise('close', blockTopicDialog);
    await flushTasks();
    // Make sure we still have 3 active topics
    assertEquals(
        3,
        currentTopicsSection.querySelectorAll('privacy-sandbox-interest-item')
            .length);

    // Setting topic state to reflect blocking parent topic
    testPrivacySandboxBrowserProxy.setTestTopicState(
        {
          topTopics: [
            {
              topicId: 4,
              taxonomyVersion: 1,
              displayString: 'test-topic-4',
              description: '',
            },
          ],
          blockedTopics: [
            {
              topicId: 1,
              taxonomyVersion: 1,
              displayString: 'test-topic-1',
              description: 'test-topic-1-description',
            },
            {
              topicId: 2,
              taxonomyVersion: 1,
              displayString: 'test-topic-2',
              description: '',
            },
          ],
        },
    );

    // Try blocking topic again
    blockButton!.click();
    await flushTasks();
    blockTopicDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#blockTopicDialog');
    assertTrue(!!blockTopicDialog);
    await (whenAttributeIs(blockTopicDialog.$.dialog, 'open', ''));
    blockTopicDialog.$.confirm.click();
    await eventToPromise('close', blockTopicDialog);
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    const expandedButton =
        page.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#blockedTopicsRow');
    assert(expandedButton);
    assertTrue(expandedButton.expanded);
    await waitAfterNextRender(page);
    assertEquals(blockedTopicsRow, page.shadowRoot!.activeElement);
    // Assert the topic AND it's child topic is no longer visible.
    assertEquals(
        1,
        currentTopicsSection.querySelectorAll('privacy-sandbox-interest-item')
            .length);
    assert(!!currentTopicsSection
                 .querySelectorAll('privacy-sandbox-interest-item')[0]!
                 .shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-4',
        currentTopicsSection
            .querySelectorAll('privacy-sandbox-interest-item')[0]!.shadowRoot!
            .querySelector('#label')!.textContent);

    // Setting topic state to reflect blocking the last active topic
    testPrivacySandboxBrowserProxy.setTestTopicState(
        {
          topTopics: [],
          blockedTopics: [
            {
              topicId: 1,
              taxonomyVersion: 1,
              displayString: 'test-topic-1',
              description: 'test-topic-1-description',
            },
            {
              topicId: 2,
              taxonomyVersion: 1,
              displayString: 'test-topic-2',
              description: '',
            },
            {
              topicId: 4,
              taxonomyVersion: 1,
              displayString: 'test-topic-4',
              description: '',
            },
          ],
        },
    );

    testPrivacySandboxBrowserProxy.setChildTopics([]);
    blockButton = items[2]?.shadowRoot!.querySelector('cr-button');
    blockButton!.click();
    await flushTasks();
    assertEquals(
        0,
        currentTopicsSection.querySelectorAll('privacy-sandbox-interest-item')
            .length);
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    assertTrue(isVisible(currentTopicsSection.querySelector(
        '#currentTopicsDescriptionEmptyTextHeading')));
    assertTrue(isVisible(currentTopicsSection.querySelector(
        '#currentTopicsDescriptionEmptyTextV2')));

    // Check that the focus is not lost after blocking the last item.
    await waitAfterNextRender(page);
    assertEquals(blockedTopicsRow, page.shadowRoot!.activeElement);

    // Assert the topic was moved to blocked topics section.
    blockedTopics =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(3, blockedTopics.length);
    assert(!!blockedTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-1',
        blockedTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);
    assert(!!blockedTopics[1]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-2',
        blockedTopics[1]!.shadowRoot!.querySelector('#label')!.textContent);
    assert(!!blockedTopics[2]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-4',
        blockedTopics[2]!.shadowRoot!.querySelector('#label')!.textContent);

    // Setting topic state to reflect allowing a blocked topic
    testPrivacySandboxBrowserProxy.setTestTopicState(
        {
          topTopics: [],
          blockedTopics: [
            {
              topicId: 2,
              taxonomyVersion: 1,
              displayString: 'test-topic-2',
              description: '',
            },
            {
              topicId: 4,
              taxonomyVersion: 1,
              displayString: 'test-topic-4',
              description: '',
            },
          ],
        },
    );

    // Allow first blocked topic.
    let blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    // When the parent topic was blocked, the child topic does not get moved
    // to the blocked items list which is why we only have 3 blocked topics
    assertEquals(3, blockedItems.length);
    const unblockButton =
        blockedItems[0]!.shadowRoot!.querySelector('cr-button');
    assert(unblockButton);
    assertEquals('Unblock', unblockButton.innerText);
    assertEquals(
        'Unblock test-topic-1', unblockButton.getAttribute('aria-label'));
    unblockButton.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    assertToastOpened();

    // Setting topic state to reflect allowing another blocked topic
    testPrivacySandboxBrowserProxy.setTestTopicState(
        {
          topTopics: [],
          blockedTopics: [{
            topicId: 4,
            taxonomyVersion: 1,
            displayString: 'test-topic-4',
            description: '',
          }],
        },
    );

    // Allow second blocked topic.
    blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, blockedItems.length);
    assert(!!blockedTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-2',
        blockedTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertToastOpened();

    // Setting topic state to reflect allowing the last blocked topic
    testPrivacySandboxBrowserProxy.setTestTopicState(
        {
          topTopics: [],
          blockedTopics: [],
        },
    );

    // Allow third blocked topic
    blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(1, blockedItems.length);
    assert(!!blockedTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-4',
        blockedTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertToastOpened();
    // Assert all blocked topics are gone.
    assertEquals(
        0,
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item')
            .length);

    // Check that the focus is not lost after allowing the last item.
    await waitAfterNextRender(page);
    assertEquals(blockedTopicsRow, page.shadowRoot!.activeElement);
    // Check that blocked topics empty text appears
    assertTrue(
        isChildVisible(page, '#blockedTopicsDescriptionEmptyTextHeading'));
    assertTrue(isChildVisible(page, '#blockedTopicsDescriptionEmptyTextV2'));
  });

  test('topicsManaged', async function() {
    page.set('prefs.privacy_sandbox.m1.topics_enabled', {
      ...page.get('prefs.privacy_sandbox.m1.topics_enabled'),
      value: false,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    await flushTasks();
    assertFalse(page.$.topicsToggle.checked);
    assertTrue(page.$.topicsToggle.controlDisabled());
    assertFalse(isChildVisible(page, '#currentTopicsSection'));
  });

  test('footerLinks', async function() {
    assertTrue(isChildVisible(page, '#footerV2'));
    const links = page.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
        '#footerV2 a[href]');
    assertEquals(links.length, 3, 'footer should contains three links');
    links.forEach(
        link => assertEquals(
            link.getAttribute('aria-description'),
            loadTimeData.getString('opensInNewTab'),
            'the link should indicate that it will be opened in a new tab'));
    const hrefs = Array.from<HTMLAnchorElement>(links).map(link => link.href);
    const expectedLinks = [
      'chrome://settings/adPrivacy/sites',
      'chrome://settings/cookies',
      'https://support.google.com/chrome?p=ad_privacy',
    ];
    assertDeepEquals(hrefs, expectedLinks);
  });

  test('manageTopicsRow', async function() {
    const manageTopicsRow = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#privacySandboxManageTopicsLinkRow');
    assertTrue(!!manageTopicsRow);
    assertTrue(isVisible(manageTopicsRow));
    assertEquals(
        loadTimeData.getString('manageTopicsHeading'), manageTopicsRow.label);
    assertEquals(
        loadTimeData.getString('manageTopicsDescription'),
        manageTopicsRow.subLabel);
  });

  test('clickManageTopicsRow', async function() {
    const manageTopicsRow = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#privacySandboxManageTopicsLinkRow');
    assertTrue(!!manageTopicsRow);
    manageTopicsRow.click();
    assertEquals(
        routes.PRIVACY_SANDBOX_MANAGE_TOPICS,
        Router.getInstance().getCurrentRoute());
  });

  test('navigateToManageTopicsPrefDisabled', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const manageTopicsPage = document.createElement(
        'settings-privacy-sandbox-manage-topics-subpage');
    manageTopicsPage.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_MANAGE_TOPICS);
    document.body.appendChild(manageTopicsPage);
    assertEquals(
        Router.getInstance().getCurrentRoute(), routes.PRIVACY_SANDBOX_TOPICS);
  });
});

suite('ManageTopics', function() {
  let page: SettingsPrivacySandboxManageTopicsSubpageElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    testPrivacySandboxBrowserProxy.setFirstLevelTopicsState(
        getFirstLevelTopicsState());
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement(
        'settings-privacy-sandbox-manage-topics-subpage');
    page.prefs = settingsPrefs.prefs!;
    page.set('prefs.privacy_sandbox.m1.topics_enabled', {value: true});
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_MANAGE_TOPICS);
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getFirstLevelTopics');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  function getFirstLevelTopicsState(): FirstLevelTopicsState {
    return {
      firstLevelTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
        {
          topicId: 4,
          taxonomyVersion: 1,
          displayString: 'test-topic-4',
          description: 'test-topic-4-description',
        },
      ],
      blockedTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
        {
          topicId: 2,
          taxonomyVersion: 1,
          displayString: 'test-topic-2',
          description: '',
        },
      ],
    };
  }

  test('ManageTopicsPageTestExplanationText', async function() {
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    const manageTopicsExplanationText =
        page.shadowRoot!.querySelector('#explanationText');
    assertTrue(!!manageTopicsExplanationText);
    assertTrue(isVisible(manageTopicsExplanationText));
    const links = page.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
        '#explanationText a[href]');
    assertEquals(
        links.length, 1, 'Explanation text should have one Learn more link');
    assertEquals(
        links[0]!.getAttribute('aria-description'),
        loadTimeData.getString('opensInNewTab'),
        'the link should indicate that it will be opened in a new tab');
    assertEquals(
        links[0]!.getAttribute('aria-label'),
        'Learn more about managing your ad privacy in Chrome.');
    assertEquals(
        'https://support.google.com/chrome?p=ad_privacy', links[0]!.href);
    const learnMoreLink =
        manageTopicsExplanationText.querySelector<HTMLElement>(
            '#learnMoreLink');
    assertTrue(!!learnMoreLink);
    learnMoreLink.click();
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.LearnMoreClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('ManageTopicsPageTestLabelsAndSubLabels', async function() {
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    const firstLevelTopics =
        page.shadowRoot!.querySelectorAll('.topic-toggle-row');
    assertEquals(2, firstLevelTopics.length);
    const labels = Array.from(page.shadowRoot!.querySelectorAll('.label'))
                       .map(label => label.textContent);
    assertDeepEquals(['test-topic-1', 'test-topic-4'], labels);
    const subLabels =
        Array.from(page.shadowRoot!.querySelectorAll('.sub-label-text'))
            .map(subLabel => subLabel.textContent);
    assertDeepEquals(
        ['test-topic-1-description', 'test-topic-4-description'], subLabels);
  });

  test('ManageTopicsPageTestToggles', async function() {
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    const toggles = page.shadowRoot!.querySelectorAll('cr-toggle');
    assertEquals(2, toggles.length);
    const toggleAriaLabels =
        Array.from(toggles).map(toggle => toggle.getAttribute('aria-label'));
    assertDeepEquals(['test-topic-1', 'test-topic-4'], toggleAriaLabels);
    const toggleAriaDescriptions = Array.from(toggles).map(
        toggle => toggle.getAttribute('aria-description'));
    assertDeepEquals(
        ['test-topic-1-description', 'test-topic-4-description'],
        toggleAriaDescriptions);
    const toggleIds = Array.from(toggles).map(topicToggle => topicToggle.id);
    assertDeepEquals(['toggle-1', 'toggle-4'], toggleIds);
    // Toggle 1 (topic 1) is also blocked so it is toggled OFF.
    assertFalse(toggles[0]!.checked);
    // Toggle 2 (topic 4) is not blocked so it is toggled ON.
    assertTrue(toggles[1]!.checked);
  });

  test('ManageTopicsPageChangeToggle', async function() {
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    testPrivacySandboxBrowserProxy.setChildTopics([{
      topicId: 3,
      taxonomyVersion: 1,
      displayString: 'test-topic-3',
      description: '',
    }]);
    // Unblocking topic 1, toggle should now be checked meaning it's unblocked.
    const toggles = page.shadowRoot!.querySelectorAll('cr-toggle');
    assertEquals(2, toggles.length);
    toggles[0]!.click();
    assertTrue(toggles[0]!.checked);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicEnabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    // Attempting to block topic 1, causes a dialog to open due to
    // getChildTopicsCurrentlyAssigned returning a non empty list of
    // child topics that would be blocked if they chose to continue.
    toggles[0]!.click();
    await flushTasks();

    let blockTopicDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#blockTopicDialog');
    assertTrue(!!blockTopicDialog);
    await (whenAttributeIs(blockTopicDialog.$.dialog, 'open', ''));

    blockTopicDialog.$.cancel.click();
    await eventToPromise('close', blockTopicDialog);
    await flushTasks();

    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlockingCanceled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    // After closing the dialog and choosing to not block it, the
    // toggle is turned back ON.
    assertTrue(toggles[0]!.checked);

    // Attempt to block topic 1 again
    toggles[0]!.click();
    await flushTasks();
    blockTopicDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#blockTopicDialog');
    assertTrue(!!blockTopicDialog);
    await (whenAttributeIs(blockTopicDialog.$.dialog, 'open', ''));

    blockTopicDialog.$.confirm.click();
    await eventToPromise('close', blockTopicDialog);
    await flushTasks();

    // The block button blocks the topic and changes the
    // toggle to be turned OFF.
    assertFalse(toggles[0]!.checked);
    assertEquals(2, metricsBrowserProxy.getArgs('recordAction').length);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlockingConfirmed',
        metricsBrowserProxy.getArgs('recordAction')[0]);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlocked',
        metricsBrowserProxy.getArgs('recordAction')[1]);
    metricsBrowserProxy.resetResolver('recordAction');

    testPrivacySandboxBrowserProxy.setChildTopics([]);
    // Toggle 2 (topic 4) has no child topics
    // that are currently assigned which is why the
    // dialog does not appear and the toggle is turned OFF.
    toggles[1]!.click();
    await flushTasks();
    assertFalse(toggles[1]!.checked);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlocked',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('ManageTopicsPageClickOnToggleRow', async function() {
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    testPrivacySandboxBrowserProxy.setChildTopics([{
      topicId: 3,
      taxonomyVersion: 1,
      displayString: 'test-topic-3',
      description: '',
    }]);
    // Unblocking topic 1, toggle should now be checked meaning it's unblocked.
    const topicToggleRows =
        page.shadowRoot!.querySelectorAll<HTMLElement>('.topic-toggle-row');
    const toggles = page.shadowRoot!.querySelectorAll('cr-toggle');
    assertEquals(2, topicToggleRows.length);
    assertEquals(2, toggles.length);
    topicToggleRows[0]!.click();
    assertTrue(toggles[0]!.checked);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicEnabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Attempting to block topic 1, causes a dialog to open due to
    // getChildTopicsCurrentlyAssigned returning a non empty list of child
    // topics that would be blocked if they choose to continue.
    topicToggleRows[0]!.click();
    await flushTasks();

    let blockTopicDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#blockTopicDialog');
    assertTrue(!!blockTopicDialog);
    await (whenAttributeIs(blockTopicDialog.$.dialog, 'open', ''));

    blockTopicDialog.$.cancel.click();
    await eventToPromise('close', blockTopicDialog);
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlockingCanceled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // After closing the dialog and choosing to not block it, the toggle is
    // turned back ON.
    assertTrue(toggles[0]!.checked);

    // Attempt to block topic 1 again
    topicToggleRows[0]!.click();
    await flushTasks();
    blockTopicDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#blockTopicDialog');
    assertTrue(!!blockTopicDialog);
    await (whenAttributeIs(blockTopicDialog.$.dialog, 'open', ''));

    blockTopicDialog.$.confirm.click();
    await eventToPromise('close', blockTopicDialog);
    await flushTasks();

    // The block button blocks the topic and changes the toggle to be turned
    // OFF.
    assertFalse(toggles[0]!.checked);
    assertEquals(2, metricsBrowserProxy.getArgs('recordAction').length);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlockingConfirmed',
        metricsBrowserProxy.getArgs('recordAction')[0]);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlocked',
        metricsBrowserProxy.getArgs('recordAction')[1]);
    metricsBrowserProxy.resetResolver('recordAction');

    testPrivacySandboxBrowserProxy.setChildTopics([]);
    // Toggle 2 (topic 4) has no child topics that are currently assigned which
    // is why the dialog does not appear and the toggle is turned OFF.
    topicToggleRows[1]!.click();
    await flushTasks();
    assertFalse(toggles[1]!.checked);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlocked',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });
});

suite('ManageTopicsAndAdTopicsPageState', function() {
  let adTopicsPage: SettingsPrivacySandboxTopicsSubpageElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    testPrivacySandboxBrowserProxy.setFirstLevelTopicsState(
        getInitialFirstLevelTopicsState());
    testPrivacySandboxBrowserProxy.setTestTopicState(getInitialTopicsState());
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.appendChild(settingsPrefs);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    adTopicsPage =
        document.createElement('settings-privacy-sandbox-topics-subpage');
    adTopicsPage.prefs = settingsPrefs.prefs;
    adTopicsPage.set('prefs.privacy_sandbox.m1.topics_enabled', {value: true});
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_TOPICS);
    document.body.appendChild(adTopicsPage);
    await testPrivacySandboxBrowserProxy.whenCalled('getTopicsState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  function getInitialTopicsState(): TopicsState {
    return {
      topTopics: [],
      blockedTopics: [{
        topicId: 1,
        taxonomyVersion: 1,
        displayString: 'test-topic-1',
        description: 'test-topic-1-description',
      }],
    };
  }

  function getInitialFirstLevelTopicsState(): FirstLevelTopicsState {
    return {
      firstLevelTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
        {
          topicId: 2,
          taxonomyVersion: 1,
          displayString: 'test-topic-2',
          description: 'test-topic-2-description',
        },
      ],
      blockedTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
      ],
    };
  }

  function getFinalTopicsState(): TopicsState {
    return {
      topTopics: [],
      blockedTopics: [
        {
          topicId: 2,
          taxonomyVersion: 1,
          displayString: 'test-topic-2',
          description: 'test-topic-2-description',
        },
      ],
    };
  }

  function getFinalFirstLevelTopicsState(): FirstLevelTopicsState {
    return {
      firstLevelTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
        {
          topicId: 2,
          taxonomyVersion: 1,
          displayString: 'test-topic-2',
          description: 'test-topic-2-description',
        },
      ],
      blockedTopics: [
        {
          topicId: 1,
          taxonomyVersion: 1,
          displayString: 'test-topic-1',
          description: 'test-topic-1-description',
        },
        {
          topicId: 2,
          taxonomyVersion: 1,
          displayString: 'test-topic-2',
          description: 'test-topic-2-description',
        },
      ],
    };
  }

  test('BlockAndAllowVerifyUpToDateState', async function() {
    // Start with Ad Topics Page.
    adTopicsPage.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    let blockedTopicsRow = adTopicsPage.shadowRoot!.querySelector<HTMLElement>(
        '#blockedTopicsRow');
    blockedTopicsRow!.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.BlockedTopicsOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Assert initial values
    let blockedTopicsList =
        adTopicsPage.shadowRoot!.querySelector('#blockedTopicsList')!;
    let blockedTopics =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertTrue(!!blockedTopics);
    assertEquals(1, blockedTopics.length);
    assert(!!blockedTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-1',
        blockedTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);

    // Navigate to Manage Topics Page.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const manageTopicsPage = document.createElement(
        'settings-privacy-sandbox-manage-topics-subpage');
    manageTopicsPage.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_MANAGE_TOPICS);
    document.body.appendChild(manageTopicsPage);
    await testPrivacySandboxBrowserProxy.whenCalled('getFirstLevelTopics');
    flushTasks();

    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Assert initial values and unblock test-topic-1 and block test-topic-2.
    let toggles = manageTopicsPage.shadowRoot!.querySelectorAll('cr-toggle');
    assertEquals(2, toggles.length);
    let toggleIds = Array.from(toggles).map(topicToggle => topicToggle.id);
    assertDeepEquals(['toggle-1', 'toggle-2'], toggleIds);
    assertFalse(toggles[0]!.checked);
    assertTrue(toggles[1]!.checked);
    toggles[0]!.click();
    assertTrue(toggles[0]!.checked);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicEnabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    toggles[1]!.click();
    assertFalse(toggles[1]!.checked);
    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlocked',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Setting TopicState to reflect changes in Manage Topics page.
    testPrivacySandboxBrowserProxy.setTestTopicState(getFinalTopicsState());

    // Navigate back to Ad Topics Page
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_TOPICS);
    document.body.appendChild(adTopicsPage);
    await testPrivacySandboxBrowserProxy.whenCalled('getTopicsState');
    flushTasks();

    // Check that unblocking test-topic-1 and blocking test-topic-2 in Manage
    // Topics Page are reflected in Ad Topics Page.
    blockedTopicsRow = adTopicsPage.shadowRoot!.querySelector<HTMLElement>(
        '#blockedTopicsRow');
    blockedTopicsRow!.click();
    await flushTasks();
    blockedTopicsList =
        adTopicsPage.shadowRoot!.querySelector('#blockedTopicsList')!;
    blockedTopics =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertTrue(!!blockedTopics);
    assertEquals(1, blockedTopics.length);
    assert(!!blockedTopics[0]!.shadowRoot!.querySelector('#label'));
    assertEquals(
        'test-topic-2',
        blockedTopics[0]!.shadowRoot!.querySelector('#label')!.textContent);
    blockedTopics[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');

    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Setting FirstLevelTopicsState to reflect changes in Ad Topics Page.
    testPrivacySandboxBrowserProxy.setFirstLevelTopicsState(
        getFinalFirstLevelTopicsState());

    // Navigate back to Manage Topics Page.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_MANAGE_TOPICS);
    document.body.appendChild(manageTopicsPage);
    await testPrivacySandboxBrowserProxy.whenCalled('getFirstLevelTopics');
    flushTasks();

    assertEquals(
        'Settings.PrivacySandbox.Topics.Manage.PageOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Confirm that blocking test-topic-2 in Ad Topics Page are reflected in
    // Manage Topics Page. Both topics should be unchecked (blocked).
    toggles = manageTopicsPage.shadowRoot!.querySelectorAll('cr-toggle');
    assertEquals(2, toggles.length);
    toggleIds = Array.from(toggles).map(topicToggle => topicToggle.id);
    assertDeepEquals(['toggle-1', 'toggle-2'], toggleIds);
    assertFalse(toggles[0]!.checked);
    assertFalse(toggles[1]!.checked);
  });
});

suite('FledgeSubpageEmpty', function() {
  let page: SettingsPrivacySandboxFledgeSubpageElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    testPrivacySandboxBrowserProxy.setFledgeState({
      joiningSites: [],
      blockedSites: [],
    });
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-fledge-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getFledgeState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('fledgeDisabled', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', false);
    await flushTasks();
    // Check the current sites descriptions.
    assertTrue(isChildVisible(page, '#currentSitesDescription'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentSitesDescriptionDisabled'));
  });

  test('fledgeEnabled', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    // Check the current sites descriptions.
    assertTrue(isChildVisible(page, '#currentSitesDescription'));
    assertTrue(isChildVisible(page, '#currentSitesDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionDisabled'));
    assertFalse(isChildVisible(page, '#seeAllSites'));
    // Check that there are no current sites.
    const currentSites =
        page.shadowRoot!.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(0, currentSites.length);
  });

  test('blockedSitesEmpty', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    const blockedSitesRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedSitesRow')!;
    const blockedSitesDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedSitesDescription')!;
    assertTrue(isVisible(blockedSitesRow));
    assertFalse(isVisible(blockedSitesDescription));
    blockedSitesRow.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Fledge.BlockedSitesOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Check the blocked sites description.
    assertTrue(isVisible(blockedSitesDescription));
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescriptionEmpty'),
        blockedSitesDescription.innerText);

    // Check that there are no blocked sites.
    const blockedSitesList =
        page.shadowRoot!.querySelector('#blockedSitesList')!;
    const blockedSites =
        blockedSitesList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(0, blockedSites.length);
  });
});

suite('FledgeSubpageSeeAllSites', function() {
  let page: SettingsPrivacySandboxFledgeSubpageElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  const sitesList: string[] = [];
  const sitesCount: number =
      SettingsPrivacySandboxFledgeSubpageElement.maxFledgeSites + 2;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    // Setup long list of sites.
    for (let i = 0; i < sitesCount; i++) {
      sitesList.push(`site-${i}.com`);
    }
    testPrivacySandboxBrowserProxy.setFledgeState({
      joiningSites: sitesList,
      blockedSites: [],
    });
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-fledge-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getFledgeState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('blockAndAllowSites', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    // Check for current sites.
    const currentSitesSection =
        page.shadowRoot!.querySelector<HTMLElement>('#currentSitesSection')!;
    const currentSites = currentSitesSection.querySelector('dom-repeat');
    assertTrue(!!currentSites);
    assertEquals(
        SettingsPrivacySandboxFledgeSubpageElement.maxFledgeSites,
        currentSites.items!.length);
    assertFalse(isVisible(
        currentSitesSection.querySelector('#currentSitesDescriptionEmpty')));

    // Check for blocked sites.
    page.shadowRoot!.querySelector<HTMLElement>('#blockedSitesRow')!.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Fledge.BlockedSitesOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    const blockedSitesList =
        page.shadowRoot!.querySelector('#blockedSitesList')!;
    const blockedSites = blockedSitesList.querySelector('dom-repeat');
    assertTrue(!!blockedSites);
    assertEquals(0, blockedSites!.items!.length);
    const blockedSitesDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedSitesDescription')!;
    assertTrue(isVisible(blockedSitesDescription));
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescriptionEmpty'),
        blockedSitesDescription.innerText);

    // Check for "See all sites" button.
    const seeAllSites =
        currentSitesSection.querySelector<HTMLElement>('#seeAllSites');
    assertTrue(isVisible(seeAllSites));
    seeAllSites!.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Fledge.AllSitesOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    const allCurrentSites = currentSitesSection.querySelectorAll('dom-repeat');
    assertTrue(!!currentSites);
    assertEquals(2, allCurrentSites.length);
    const mainSitesList = allCurrentSites[0];
    const remainingSitesList = allCurrentSites[1];
    assertEquals(
        SettingsPrivacySandboxFledgeSubpageElement.maxFledgeSites,
        mainSitesList!.items!.length);
    assertEquals(2, remainingSitesList!.items!.length);
    assertEquals(sitesList[0], mainSitesList!.items![0].site!);
    assertEquals(
        sitesList[sitesCount - 2], remainingSitesList!.items![0].site!);

    // Block site from the main current sites section.
    let items =
        currentSitesSection!.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(sitesCount, items.length);
    items[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setFledgeJoiningAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Fledge.SiteRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Check that a site from "See all sites" section move to the main one.
    assertEquals(
        SettingsPrivacySandboxFledgeSubpageElement.maxFledgeSites,
        mainSitesList!.items!.length);
    assertEquals(1, remainingSitesList!.items!.length);
    assertEquals(sitesList[1], mainSitesList!.items![0].site!);
    assertEquals(sitesList[sitesCount - 2], mainSitesList!.items!.at(-1).site!);
    assertEquals(
        sitesList[sitesCount - 1], remainingSitesList!.items![0].site!);
    items = mainSitesList!.querySelectorAll('privacy-sandbox-interest-item');

    // Check that site was blocked.
    assertEquals(1, blockedSites.items!.length);
    assertEquals(sitesList[0], blockedSites.items![0].site!);
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescription'),
        blockedSitesDescription.innerText);

    // Block site from the "See all sites" section.
    items =
        currentSitesSection!.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(sitesCount - 1, items.length);
    items[SettingsPrivacySandboxFledgeSubpageElement.maxFledgeSites]!
        .shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setFledgeJoiningAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Fledge.SiteRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Check that "See all sites" section was removed.
    assertFalse(isChildVisible(page, '#seeAllSites'));

    // Check that site was blocked.
    assertEquals(2, blockedSites.items!.length);
    assertEquals(sitesList[0], blockedSites.items![0].site!);
    assertEquals(sitesList[sitesCount - 1], blockedSites.items![1].site!);

    // Allow first blocked site.
    let blockedItems =
        blockedSitesList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, blockedItems.length);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setFledgeJoiningAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Fledge.SiteAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Allow second blocked site.
    blockedItems =
        blockedSitesList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(1, blockedItems.length);
    assertEquals(sitesList[sitesCount - 1], blockedSites.items![0].site!);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setFledgeJoiningAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Fledge.SiteAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Assert all blocked sites are gone.
    assertEquals(
        0, blockedSitesList.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(blockedSitesDescription));
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescriptionEmpty'),
        blockedSitesDescription.innerText);
  });
});

suite('AdMeasurementSubpage', function() {
  let page: SettingsPrivacySandboxAdMeasurementSubpageElement;
  let settingsPrefs: SettingsPrefsElement;
  let hatsBrowserProxy: TestHatsBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    hatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(hatsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement(
        'settings-privacy-sandbox-ad-measurement-subpage');
    page.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_AD_MEASUREMENT);
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('hatsSurveyRequested', async function() {
    const result =
        await hatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_AD_MEASUREMENT_SUBPAGE, result);
  });

  test('enableAdMeasurementToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.ad_measurement_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertFalse(page.$.adMeasurementToggle.checked);
    assertFalse(page.$.adMeasurementToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);

    page.$.adMeasurementToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertTrue(page.$.adMeasurementToggle.checked);
    assertFalse(page.$.adMeasurementToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);
    assertTrue(
        !!page.getPref('privacy_sandbox.m1.ad_measurement_enabled.value'));
    assertEquals(
        'Settings.PrivacySandbox.AdMeasurement.Enabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('disableAdMeasurementToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.ad_measurement_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertTrue(page.$.adMeasurementToggle.checked);
    assertFalse(page.$.adMeasurementToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);

    page.$.adMeasurementToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertFalse(page.$.adMeasurementToggle.checked);
    assertFalse(page.$.adMeasurementToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);
    assertFalse(
        !!page.getPref('privacy_sandbox.m1.ad_measurement_enabled.value'));
    assertEquals(
        'Settings.PrivacySandbox.AdMeasurement.Disabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('adMeasurementManaged', async function() {
    page.set('prefs.privacy_sandbox.m1.ad_measurement_enabled', {
      ...page.get('prefs.privacy_sandbox.m1.ad_measurement_enabled'),
      value: false,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    await flushTasks();
    assertFalse(page.$.adMeasurementToggle.checked);
    assertTrue(page.$.adMeasurementToggle.controlDisabled());
  });
});
