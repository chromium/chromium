// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {SettingsPrivacySandboxPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
