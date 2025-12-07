// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsAiLoggingInfoBullet} from 'chrome://settings/settings.js';
import {loadTimeData, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

suite('LoggingInfoBullet', function() {
  let row: SettingsAiLoggingInfoBullet;

  function createRow(
      pref: PrefObject, loggingManagedDisabledCustomLabel: string|null = null) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    row = document.createElement('settings-ai-logging-info-bullet');
    row.pref = pref;
    if (loggingManagedDisabledCustomLabel) {
      row.loggingManagedDisabledCustomLabel = loggingManagedDisabledCustomLabel;
    }
    document.body.appendChild(row);
    return flushTasks();
  }

  test('infoBulletPolicyAllow', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    };
    await createRow(pref);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(
        loadTimeData.getString('aiSubpageSublabelReviewers'), li.textContent);
    assertTrue(!!li.querySelector('cr-icon'));
    assertFalse(!!li.querySelector('cr-policy-pref-indicator'));
  });

  test('infoBulletPolicyAllowCustomLabel', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    };
    await createRow(
        pref,
        loadTimeData.getString(
            'autofillAiSubpageSublabelLoggingManagedDisabled'));

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    // The custom label is not used since it only applies when logging is
    // disabled.
    assertEquals(
        loadTimeData.getString('aiSubpageSublabelReviewers'), li.textContent);
  });

  test('infoBulletPolicyAllowWithoutLogging', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    };
    await createRow(pref);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(
        loadTimeData.getString('aiSubpageSublabelLoggingManagedDisabled'),
        li.textContent);
    assertFalse(!!li.querySelector('cr-icon'));
    assertTrue(!!li.querySelector('cr-policy-pref-indicator'));
  });

  test('infoBulletPolicyAllowWithoutLoggingCustomLabel', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    };
    const customLabel = loadTimeData.getString(
        'autofillAiSubpageSublabelLoggingManagedDisabled');
    await createRow(pref, customLabel);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(customLabel, li.textContent);
  });

  test('infoBulletPolicyDisabled', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.DISABLE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    };
    await createRow(pref);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(
        loadTimeData.getString('aiSubpageSublabelLoggingManagedDisabled'),
        li.textContent);
    assertFalse(!!li.querySelector('cr-icon'));
    assertTrue(!!li.querySelector('cr-policy-pref-indicator'));
  });

  test('infoBulletPolicyDisabledCustomLabel', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.DISABLE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    };
    const customLabel = loadTimeData.getString(
        'autofillAiSubpageSublabelLoggingManagedDisabled');
    await createRow(pref, customLabel);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(customLabel, li.textContent);
    assertFalse(!!li.querySelector('cr-icon'));
    assertTrue(!!li.querySelector('cr-policy-pref-indicator'));
  });
});
