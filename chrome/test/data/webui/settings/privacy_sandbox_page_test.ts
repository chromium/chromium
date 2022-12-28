// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrDialogElement, SettingsPrivacySandboxAdMeasurementSubpageElement, SettingsPrivacySandboxFledgeSubpageElement, SettingsPrivacySandboxPageElement, SettingsPrivacySandboxTopicsSubpageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrivacySandboxBrowserProxyImpl, Router, routes, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPrivacySandboxBrowserProxy} from './test_privacy_sandbox_browser_proxy.js';

suite('PrivacySandboxPageTests', function() {
  let page: SettingsPrivacySandboxPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
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
    assertTrue(isVisible(page.$.privacySandboxTopicsLinkRow));
    assertTrue(isVisible(page.$.privacySandboxFledgeLinkRow));
    assertTrue(isVisible(page.$.privacySandboxAdMeasurementLinkRow));
  });

  test('privacySandboxTopicsRowSublabel', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.privacySandboxTopicsLinkRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageTopicsLinkRowSubLabelEnabled'),
        page.$.privacySandboxTopicsLinkRow.subLabel);

    page.setPrefValue('privacy_sandbox.m1.topics_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.privacySandboxTopicsLinkRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageTopicsLinkRowSubLabelDisabled'),
        page.$.privacySandboxTopicsLinkRow.subLabel);
  });

  test('privacySandboxFledgeRowSublabel', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.privacySandboxFledgeLinkRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageFledgeLinkRowSubLabelEnabled'),
        page.$.privacySandboxFledgeLinkRow.subLabel);

    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.privacySandboxFledgeLinkRow));
    assertEquals(
        loadTimeData.getString('adPrivacyPageFledgeLinkRowSubLabelDisabled'),
        page.$.privacySandboxFledgeLinkRow.subLabel);
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

  test('clickPrivacySandboxTopicsLinkRow', function() {
    page.$.privacySandboxTopicsLinkRow.click();
    assertEquals(
        routes.PRIVACY_SANDBOX_TOPICS, Router.getInstance().getCurrentRoute());
  });

  test('clickPrivacySandboxFledgeLinkRow', function() {
    page.$.privacySandboxFledgeLinkRow.click();
    assertEquals(
        routes.PRIVACY_SANDBOX_FLEDGE, Router.getInstance().getCurrentRoute());
  });

  test('clickPrivacySandboxAdMeasurementLinkRow', function() {
    page.$.privacySandboxAdMeasurementLinkRow.click();
    assertEquals(
        routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
        Router.getInstance().getCurrentRoute());
  });
});

suite('PrivacySandboxTopicsSubpageTests', function() {
  let page: SettingsPrivacySandboxTopicsSubpageElement;
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
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);

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
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertFalse(isChildVisible(page, '#currentTopicsDescription'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionDisabled'));

    page.$.topicsToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertTrue(!!page.getPref('privacy_sandbox.m1.topics_enabled.value'));
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    // TODO(crbug.com/1378703): Add test for `#currentTopicsDescriptionEmpty`
    // when `getTopicsState()` returns an empty list.
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
  });

  test('disableTopicsToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertTrue(page.$.topicsToggle.checked);
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertTrue(isChildVisible(page, '#currentTopicsDescription'));
    // TODO(crbug.com/1378703): Add test for `#currentTopicsDescriptionEmpty`
    // when `getTopicsState()` returns an empty list.
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionDisabled'));

    page.$.topicsToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.topicsToggle));
    assertFalse(page.$.topicsToggle.checked);
    assertEquals(
        loadTimeData.getString('topicsPageToggleSubLabel'),
        page.$.topicsToggle.subLabel);
    assertFalse(!!page.getPref('privacy_sandbox.m1.topics_enabled.value'));
    assertFalse(isChildVisible(page, '#currentTopicsDescription'));
    assertFalse(isChildVisible(page, '#currentTopicsDescriptionEmpty'));
    assertTrue(isChildVisible(page, '#currentTopicsDescriptionDisabled'));
  });

  test('learnMoreDialog', async function() {
    page.setPrefValue('privacy_sandbox.m1.topics_enabled', true);
    await flushTasks();

    assertLearnMoreDialogClosed();
    const learnMoreButton =
        page.shadowRoot!.querySelector<HTMLElement>('#learnMoreLink')!;
    assertTrue(isVisible(learnMoreButton));
    learnMoreButton.click();
    await flushTasks();

    assertLearnMoreDialogOpened();
    const closeButton =
        page.shadowRoot!.querySelector<HTMLElement>('#closeButton')!;
    assertTrue(isVisible(closeButton));
    closeButton.click();
    await flushTasks();
    assertLearnMoreDialogClosed();
    await waitAfterNextRender(page);
    assertEquals(learnMoreButton, page.shadowRoot!.activeElement);
  });

  // TODO(crbug.com/1378703): Add test for empty blocked topics list description
  // when `getTopicsState()` returns an empty list.
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
    page.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow')!.click();
    await flushTasks();
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
    item.shadowRoot!.querySelector('cr-button')!.click();
    // TODO(b/263853353): Test for recorded metric for blocked topic.
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');

    // Assert the topic is no longer visible.
    assertEquals(
        0, currentTopicsSection.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(
        currentTopicsSection.querySelector('#currentTopicsDescriptionEmpty')));

    // Assert the topic was moved to blocked topics section.
    blockedTopics = blockedTopicsList.querySelector('dom-repeat')!;
    assertEquals(2, blockedTopics.items!.length);
    assertEquals('test-topic-1', blockedTopics.items![0].topic!.displayString);
    assertEquals('test-topic-2', blockedTopics.items![1].topic!.displayString);

    // Allow first blocked topic.
    let blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(2, blockedItems.length);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    // TODO(b/263853353): Test for recorded metric for allowed topic.

    // Allow second blocked topic.
    blockedItems =
        blockedTopicsList.querySelectorAll('privacy-sandbox-interest-item');
    assertEquals(1, blockedItems.length);
    assertEquals('test-topic-2', blockedTopics.items![0].topic!.displayString);
    blockedItems[0]!.shadowRoot!.querySelector('cr-button')!.click();
    await testPrivacySandboxBrowserProxy.whenCalled('setTopicAllowed');
    // TODO(b/263853353): Test for recorded metric for allowed topic.

    // Assert all blocked topics are gone.
    assertEquals(
        0, blockedTopicsList.querySelector('dom-repeat')!.items!.length);
    assertTrue(isVisible(blockedTopicsDescription));
    assertEquals(
        loadTimeData.getString('topicsPageBlockedTopicsDescriptionEmpty'),
        blockedTopicsDescription.innerText);
  });
});

suite('PrivacySandboxFledgeSubpageTests', function() {
  let page: SettingsPrivacySandboxFledgeSubpageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    page = document.createElement('settings-privacy-sandbox-fledge-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('enableFledgeToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', false);
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertFalse(page.$.fledgeToggle.checked);
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);

    page.$.fledgeToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertTrue(page.$.fledgeToggle.checked);
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);
    assertTrue(!!page.getPref('privacy_sandbox.m1.fledge_enabled.value'));
  });

  test('disableFledgeToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.fledge_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertTrue(page.$.fledgeToggle.checked);
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);

    page.$.fledgeToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.fledgeToggle));
    assertFalse(page.$.fledgeToggle.checked);
    assertEquals(
        loadTimeData.getString('fledgePageToggleSubLabel'),
        page.$.fledgeToggle.subLabel);
    assertFalse(!!page.getPref('privacy_sandbox.m1.fledge_enabled.value'));
  });
});

suite('PrivacySandboxAdMeasurementSubpageTests', function() {
  let page: SettingsPrivacySandboxAdMeasurementSubpageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
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
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);

    page.$.adMeasurementToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertTrue(page.$.adMeasurementToggle.checked);
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);
    assertTrue(
        !!page.getPref('privacy_sandbox.m1.ad_measurement_enabled.value'));
  });

  test('disableAdMeasurementToggle', async function() {
    page.setPrefValue('privacy_sandbox.m1.ad_measurement_enabled', true);
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertTrue(page.$.adMeasurementToggle.checked);
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);

    page.$.adMeasurementToggle.click();
    await flushTasks();
    assertTrue(isVisible(page.$.adMeasurementToggle));
    assertFalse(page.$.adMeasurementToggle.checked);
    assertEquals(
        loadTimeData.getString('adMeasurementPageToggleSubLabel'),
        page.$.adMeasurementToggle.subLabel);
    assertFalse(
        !!page.getPref('privacy_sandbox.m1.ad_measurement_enabled.value'));
  });
});
