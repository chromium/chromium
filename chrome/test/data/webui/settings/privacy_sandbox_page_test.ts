// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {SettingsPrivacySandboxPageElement} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

suite('PrivacySandboxPageTests', function() {
  let page: SettingsPrivacySandboxPageElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-sandbox-page');
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
