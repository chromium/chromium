// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrDialogElement, SettingsPrivacySandboxAdMeasurementSubpageElement, SettingsPrivacySandboxFledgeSubpageElement, SettingsPrivacySandboxPageElement, SettingsPrivacySandboxTopicsSubpageElement} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacySandboxBrowserProxyImpl, Router, routes, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacySandboxBrowserProxy} from './test_privacy_sandbox_browser_proxy.js';

suite('PrivacySandboxPageTests', function() {
  let page: SettingsPrivacySandboxPageElement;
  let settingsPrefs: SettingsPrefsElement;
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

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('privacySandboxLinkRowsVisible', function() {
    assertTrue(isChildVisible(page, '#privacySandboxTopicsLinkRow'));
    assertTrue(isChildVisible(page, '#privacySandboxFledgeLinkRow'));
    assertTrue(isVisible(page.$.privacySandboxAdMeasurementLinkRow));
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
    assertTrue(isVisible(page.$.privacySandboxAdMeasurementLinkRow));
    assertEquals(
        loadTimeData.getString(
            'adPrivacyPageAdMeasurementLinkRowSubLabelEnabled'),
        page.$.privacySandboxAdMeasurementLinkRow.subLabel);

    page.setPrefValue('privacy_sandbox.m1.ad_measurement_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.privacySandboxAdMeasurementLinkRow));
    assertEquals(
        loadTimeData.getString(
            'adPrivacyPageAdMeasurementLinkRowSubLabelDisabled'),
        page.$.privacySandboxAdMeasurementLinkRow.subLabel);
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
    page.$.privacySandboxAdMeasurementLinkRow.click();
    assertEquals(
        'Settings.PrivacySandbox.AdMeasurement.Opened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertEquals(
        routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
        Router.getInstance().getCurrentRoute());
  });
});

suite('PrivacySandboxNoticeRestrictedEnabledTests', function() {
  let page: SettingsPrivacySandboxPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      isPrivacySandboxRestrictedNoticeEnabled: true,
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

  // When the restricted notice is configured, ensure only measurement is shown.
  test('privacySandboxLinkRowsNotVisible', function() {
    assertFalse(isChildVisible(page, '#privacySandboxTopicsLinkRow'));
    assertFalse(isChildVisible(page, '#privacySandboxFledgeLinkRow'));
    assertTrue(isVisible(page.$.privacySandboxAdMeasurementLinkRow));
  });
});

suite('PrivacySandboxTopicsSubpageTests', function() {
  let page: SettingsPrivacySandboxTopicsSubpageElement;
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
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-topics-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getTopicsState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  function assertLearnMoreDialogClosed() {
    const dialog = page.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertFalse(!!dialog);
  }

  function assertLearnMoreDialogOpened() {
    const dialog = page.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
  }

  test('enableTopicsToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertFalse(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    assertEquals(
        0, testPrivacySandboxBrowserProxy.getCallCount('topicsToggleChanged'));

    page.$.topicsToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertTrue(!!page.getPref('privacy_sandbox.m1.topics_enabled.value'));
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    // The current list is always empty after re-enabling the toggle.
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    assertEquals(
        'Settings.PrivacySandbox.Topics.Enabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertTrue((await testPrivacySandboxBrowserProxy.whenCalled(
        'topicsToggleChanged'))[0]);
  });

  test('disableTopicsToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    assertEquals(
        0, testPrivacySandboxBrowserProxy.getCallCount('topicsToggleChanged'));

    page.$.topicsToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertFalse(page.$.topicsToggle.checked);
    assertFalse(page.$.topicsToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertFalse(!!page.getPref('privacy_sandbox.m1.topics_enabled.value'));
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    assertEquals(
        'Settings.PrivacySandbox.Topics.Disabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
    assertFalse((await testPrivacySandboxBrowserProxy.whenCalled(
        'topicsToggleChanged'))[0]);
  });

  test('learnMoreDialog', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();

    assertLearnMoreDialogClosed();
    const learnMoreButton =
        page.shadowRoot!.querySelector<HTMLElement>('#learnMoreLink')!;
    assertTrue(isVisible(learnMoreButton));
    assertEquals(
        loadTimeData.getString(
            'topicsPageCurrentTopicsDescriptionLearnMoreA11yLabel'),
        learnMoreButton.getAttribute('aria-label'));
    learnMoreButton.click();
    await flushTasks();

    assertLearnMoreDialogOpened();
    assertEquals(
        'Settings.PrivacySandbox.Topics.LearnMoreClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
    const closeButton =
        page.shadowRoot!.querySelector<HTMLElement>('#closeButton')!;
    assertTrue(isVisible(closeButton));
    closeButton.click();
    await flushTasks();

    assertLearnMoreDialogClosed();
    await waitAfterNextRender(page);
    assertEquals(learnMoreButton, page.shadowRoot!.activeElement);
  });

  test('blockedTopicsNotEmpty', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    const blockedTopicsRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow')!;
    let blockedTopicsDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedTopicsDescription')!;
    assertTrue(isVisible(blockedTopicsRow));
    assertFalse(isVisible(blockedTopicsDescription));
    blockedTopicsRow.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.BlockedTopicsOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Check that blocked topics are shown even when toggle is disabled.
    blockedTopicsDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedTopicsDescription')!;
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescription'),
        blockedTopicsDescription.innerText);
    const blockedTopicsList =
        page.shadowRoot!.querySelector('#blockedTopicsList')!;
    let blockedTopics = blockedTopicsList.querySelector('dom-repeat');
    assertTrue(!!blockedTopics);
    assertEquals(1, blockedTopics.items!.length);

    // Check that blocked topics are shown when toggle is enabled.
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    blockedTopicsDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedTopicsDescription')!;
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescription'),
        blockedTopicsDescription.innerText);
    blockedTopics = blockedTopicsList.querySelector('dom-repeat');
    assertTrue(!!blockedTopics);
    assertEquals(1, blockedTopics.items!.length);
  });

  test('blockAndAllowTopics', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    // Check for current topics.
    const currentTopicsSection =
        page.shadowRoot!.querySelector<HTMLElement>('#currentTopicsSection')!;
    const currentTopics = currentTopicsSection.querySelector('dom-repeat');
    assertTrue(!!currentTopics);
    assertEquals(1, currentTopics.items!.length);
    assertFalse(isVisible(
        currentTopicsSection.querySelector('#currentTopicsDescriptionEmpty')));
    assertEquals('test-topic-1', currentTopics.items![0].topic!.displayString);

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
    let blockedTopics = blockedTopicsList.querySelector('dom-repeat');
    assertTrue(!!blockedTopics);
    const blockedTopicsDescription =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#blockedTopicsDescription')!;
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescription'),
        blockedTopicsDescription.innerText);
    assertEquals(1, blockedTopics.items!.length);
    assertEquals('test-topic-2', blockedTopics.items![0].topic!.displayString);

    // Block topic.
    const item =
        currentTopicsSection.querySelector('privacy-sandbox-interest-item')!;
    const blockButton = item.shadowRoot!.querySelector('cr-button');
    assertEquals(
        page.i18n('topicsPageBlockTopicA11yLabel', 'test-topic-1'),
        blockButton!.getAttribute('aria-label'));
    blockButton!.click();
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');

    // Assert the topic is no longer visible.
    assertEquals(
        0, currentTopicsSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(
        currentTopicsSection.querySelector('#currentTopicsDescriptionEmpty')));

    // Check that the focus is not lost after blocking the last item.
    await waitAfterNextRender(page);
    assertEquals(blockedTopicsRow, page.shadowRoot!.activeElement);

    // Assert the topic was moved to blocked topics section.
    blockedTopics = blockedTopicsList.querySelector('dom-repeat')!;
    assertEquals(2, blockedTopics.items!.length);
    assertEquals('test-topic-1', blockedTopics.items![0].topic!.displayString);
    assertEquals('test-topic-2', blockedTopics.items![1].topic!.displayString);

    // Allow first blocked topic.
    let blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, blockedItems.length);
    const allowButton = blockedItems[0]!.shadowRoot!.querySelector('cr-button');
    assertEquals(
        page.i18n('topicsPageAllowTopicA11yLabel', 'test-topic-1'),
        allowButton!.getAttribute('aria-label'));
    allowButton!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Allow second blocked topic.
    blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(1, blockedItems.length);
    assertEquals('test-topic-2', blockedTopics.items![0].topic!.displayString);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Topics.TopicAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Assert all blocked topics are gone.
    assertEquals(
        0, blockedTopicsList.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescriptionEmpty'),
        blockedTopicsDescription.innerText);

    // Check that the focus is not lost after allowing the last item.
    await waitAfterNextRender(page);
    assertEquals(blockedTopicsRow, page.shadowRoot!.activeElement);
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
    assertTrue(isChildVisible(page, '#footer'));
    const links =
        page.shadowRoot!.querySelectorAll<HTMLAnchorElement>('#footer a[href]');
    assertEquals(links.length, 2, 'footer should contains two links');
    links.forEach(
        link => assertEquals(
            link.title, loadTimeData.getString('opensInNewTab'),
            'the link should indicate that it will be opened in a new tab'));
    const hrefs = Array.from<HTMLAnchorElement>(links).map(link => link.href);
    const expectedLinks =
        ['chrome://settings/adPrivacy/sites', 'chrome://settings/cookies'];
    assertDeepEquals(hrefs, expectedLinks);
  });
});

suite('PrivacySandboxTopicsSubpageEmptyTests', function() {
  let page: SettingsPrivacySandboxTopicsSubpageElement;
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
    testPrivacySandboxBrowserProxy.setTopicsState({
      topTopics: [],
      blockedTopics: [],
    });
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-topics-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await testPrivacySandboxBrowserProxy.whenCalled('getTopicsState');
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('topicsDisabled', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    await flushTasks();
    // Check the current topics descriptions.
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
  });

  test('topicsEnabled', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    // Check the current topics descriptions.
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
    // Check that there are no current topics.
    const currentTopics =
        page.shadowRoot!.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(0, currentTopics.length);
  });

  test('blockedTopicsEmpty', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    const blockedTopicsRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow')!;
    const blockedTopicsDescription =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#blockedTopicsDescription')!;
    assertTrue(isVisible(blockedTopicsRow));
    assertFalse(isVisible(blockedTopicsDescription));
    blockedTopicsRow.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Topics.BlockedTopicsOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Check the blocked topics description.
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescriptionEmpty'),
        blockedTopicsDescription.innerText);

    // Check that there are no blocked topics.
    const blockedTopicsList =
        page.shadowRoot!.querySelector('#blockedTopicsList')!;
    const blockedTopics =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(0, blockedTopics.length);
  });
});

suite('PrivacySandboxFledgeSubpageTests', function() {
  let page: SettingsPrivacySandboxFledgeSubpageElement;
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

  function assertLearnMoreDialogClosed() {
    const dialog = page.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertFalse(!!dialog);
  }

  function assertLearnMoreDialogOpened() {
    const dialog = page.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
  }

  test('enableFledgeToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertFalse(page.$.fledgeToggle.checked);
    assertFalse(page.$.fledgeToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);
    assertTrue(isChildVisible(page, '#currentSitesDescription'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentSitesDescriptionDisabled'));

    page.$.fledgeToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertTrue(page.$.fledgeToggle.checked);
    assertFalse(page.$.fledgeToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);
    assertTrue(!!page.getPref('privacy_sandbox.m1.fledge_enabled.value'));
    assertTrue(isChildVisible(page, '#currentSitesDescription'));
    // The current list is always empty after re-enabling the toggle.
    assertTrue(isChildVisible(page, '#currentSitesDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionDisabled'));
    assertEquals(
        'Settings.PrivacySandbox.Fledge.Enabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('disableFledgeToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertTrue(page.$.fledgeToggle.checked);
    assertFalse(page.$.fledgeToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);
    assertTrue(isChildVisible(page, '#currentSitesDescription'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionDisabled'));

    page.$.fledgeToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertFalse(page.$.fledgeToggle.checked);
    assertFalse(page.$.fledgeToggle.controlDisabled());
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);
    assertFalse(!!page.getPref('privacy_sandbox.m1.fledge_enabled.value'));
    assertTrue(isChildVisible(page, '#currentSitesDescription'));
    assertFalse(isChildVisible(page, '#currentSitesDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentSitesDescriptionDisabled'));
    assertEquals(
        'Settings.PrivacySandbox.Fledge.Disabled',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('learnMoreDialog', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();

    assertLearnMoreDialogClosed();
    const learnMoreButton =
        page.shadowRoot!.querySelector<HTMLElement>('#learnMoreLink')!;
    assertTrue(isVisible(learnMoreButton));
    assertEquals(
        loadTimeData.getString(
            'fledgePageCurrentSitesDescriptionLearnMoreA11yLabel'),
        learnMoreButton.getAttribute('aria-label'));
    learnMoreButton.click();
    await flushTasks();

    assertLearnMoreDialogOpened();
    assertEquals(
        'Settings.PrivacySandbox.Fledge.LearnMoreClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
    const closeButton =
        page.shadowRoot!.querySelector<HTMLElement>('#closeButton')!;
    assertTrue(isVisible(closeButton));
    closeButton.click();
    await flushTasks();

    assertLearnMoreDialogClosed();
    await waitAfterNextRender(page);
    assertEquals(learnMoreButton, page.shadowRoot!.activeElement);
  });

  test('blockedSitesDescriptionNotEmpty', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', false);
    const blockedSitesRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedSitesRow')!;
    let blockedSitesDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedSitesDescription')!;
    assertTrue(isVisible(blockedSitesRow));
    assertFalse(isVisible(blockedSitesDescription));
    blockedSitesRow.click();
    await flushTasks();

    assertEquals(
        'Settings.PrivacySandbox.Fledge.BlockedSitesOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    blockedSitesDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedSitesDescription')!;
    assertTrue(isVisible(blockedSitesDescription));
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescription'),
        blockedSitesDescription.innerText);

    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    blockedSitesDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedSitesDescription')!;
    assertTrue(isVisible(blockedSitesDescription));
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescription'),
        blockedSitesDescription.innerText);
  });

  test('blockAndAllowSites', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    // Check for current sites.
    const currentSitesSection =
        page.shadowRoot!.querySelector<HTMLElement>('#currentSitesSection')!;
    const currentSites = currentSitesSection.querySelector('dom-repeat');
    assertTrue(!!currentSites);
    assertEquals(1, currentSites.items!.length);
    assertFalse(isVisible(
        currentSitesSection.querySelector('#currentSitesDescriptionEmpty')));
    assertEquals('test-site-one.com', currentSites.items![0].site!);
    assertFalse(isVisible(currentSitesSection.querySelector('#seeAllSites')));

    // Check for blocked sites.
    const blockedSitesRow =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedSitesRow');
    blockedSitesRow!.click();
    await flushTasks();
    assertEquals(
        'Settings.PrivacySandbox.Fledge.BlockedSitesOpened',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');
    const blockedSitesList =
        page.shadowRoot!.querySelector('#blockedSitesList')!;
    let blockedSites = blockedSitesList.querySelector('dom-repeat');
    assertTrue(!!blockedSites);
    const blockedSitesDescription = page.shadowRoot!.querySelector<HTMLElement>(
        '#blockedSitesDescription')!;
    assertTrue(isVisible(blockedSitesDescription));
    assertEquals(
        loadTimeData.getString('fledgePageBlockedSitesDescription'),
        blockedSitesDescription.innerText);
    assertEquals(1, blockedSites.items!.length);
    assertEquals('test-site-two.com', blockedSites.items![0].site!);

    // Block site.
    const item =
        currentSitesSection.querySelector('privacy-sandbox-interest-item')!;
    const blockButton = item.shadowRoot!.querySelector('cr-button');
    assertEquals(
        page.i18n('fledgePageBlockSiteA11yLabel', 'test-site-one.com'),
        blockButton!.getAttribute('aria-label'));
    blockButton!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setFledgeJoiningAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Fledge.SiteRemoved',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Assert the site is no longer visible.
    assertEquals(
        0, currentSitesSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(
        currentSitesSection.querySelector('#currentSitesDescriptionEmpty')));

    // Check that the focus is not lost after blocking the last item.
    await waitAfterNextRender(page);
    assertEquals(blockedSitesRow, page.shadowRoot!.activeElement);

    // Assert the site was moved to blocked sites section.
    blockedSites = blockedSitesList.querySelector('dom-repeat')!;
    assertEquals(2, blockedSites.items!.length);
    assertEquals('test-site-one.com', blockedSites.items![0].site!);
    assertEquals('test-site-two.com', blockedSites.items![1].site!);

    // Allow first blocked site.
    let blockedItems =
        blockedSitesList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, blockedItems.length);
    const allowButton = blockedItems[0]!.shadowRoot!.querySelector('cr-button');
    assertEquals(
        page.i18n('fledgePageAllowSiteA11yLabel', 'test-site-one.com'),
        allowButton!.getAttribute('aria-label'));
    allowButton!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setFledgeJoiningAllowed');
    assertEquals(
        'Settings.PrivacySandbox.Fledge.SiteAdded',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.resetResolver('recordAction');

    // Allow second blocked site.
    blockedItems =
        blockedSitesList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(1, blockedItems.length);
    assertEquals('test-site-two.com', blockedSites.items![0].site!);
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

    // Check that the focus is not lost after allowing the last item.
    await waitAfterNextRender(page);
    assertEquals(blockedSitesRow, page.shadowRoot!.activeElement);
  });

  test('fledgeManaged', async function() {
    page.set('prefs.privacy_sandbox.m1.fledge_enabled', {
      ...page.get('prefs.privacy_sandbox.m1.fledge_enabled'),
      value: false,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    await flushTasks();
    assertFalse(page.$.fledgeToggle.checked);
    assertTrue(page.$.fledgeToggle.controlDisabled());
    assertFalse(isChildVisible(page, '#currentSitesSection'));
  });

  test('footerLinks', async function() {
    assertTrue(isChildVisible(page, '#footer'));
    const links =
        page.shadowRoot!.querySelectorAll<HTMLAnchorElement>('#footer a[href]');
    assertEquals(links.length, 2, 'footer should contains two links');
    links.forEach(
        link => assertEquals(
            link.title, loadTimeData.getString('opensInNewTab'),
            'the link should indicate that it will be opened in a new tab'));
    const hrefs = Array.from<HTMLAnchorElement>(links).map(link => link.href);
    const expectedLinks =
        ['chrome://settings/adPrivacy/interests', 'chrome://settings/cookies'];
    assertDeepEquals(hrefs, expectedLinks);
  });
});

suite('PrivacySandboxFledgeSubpageEmptyTests', function() {
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

suite('PrivacySandboxFledgeSubpageSeeAllSitesTests', function() {
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

suite('PrivacySandboxAdMeasurementSubpageTests', function() {
  let page: SettingsPrivacySandboxAdMeasurementSubpageElement;
  let settingsPrefs: SettingsPrefsElement;
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

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement(
        'settings-privacy-sandbox-ad-measurement-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
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
