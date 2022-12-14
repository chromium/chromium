// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SettingsPrivacySandboxAdMeasurementSubpageElement, SettingsPrivacySandboxPageElement, SettingsPrivacySandboxTopicsSubpageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

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
    page = document.createElement('settings-privacy-sandbox-topics-subpage');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

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
    // TODO(crbug.com/1378703): Test for `#currentTopicsDescription` and
    // `#currentTopicsDescriptionEmpty` separately.
    assertTrue(
        isChildVisible(page, '#currentTopicsDescription') ||
        isChildVisible(page, '#currentTopicsDescriptionEmpty'));
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
    // TODO(crbug.com/1378703): Test for `#currentTopicsDescription` and
    // `#currentTopicsDescriptionEmpty` separately.
    assertTrue(
        isChildVisible(page, '#currentTopicsDescription') ||
        isChildVisible(page, '#currentTopicsDescriptionEmpty'));
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
