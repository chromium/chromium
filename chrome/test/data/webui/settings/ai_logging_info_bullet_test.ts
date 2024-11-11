// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'chrome://settings/settings.js';

import type {SettingsAiLoggingInfoBullet} from 'chrome://settings/settings.js';
import {loadTimeData, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

suite('LoggingInfoBullet', function() {
  let row: SettingsAiLoggingInfoBullet;

  function createRow(pref: PrefObject) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    row = document.createElement('settings-ai-logging-info-bullet');
    row.pref = pref;
    document.body.appendChild(row);
    return flushTasks();
  }

  test('infoBullet', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    };
    await createRow(pref);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(
        li.innerText, loadTimeData.getString('aiSubpageSublabelReviewers'));
    assertTrue(!!li.querySelector('cr-icon'));
    assertFalse(!!li.querySelector('cr-policy-pref-indicator'));
  });

  test('infoBulletManaged', async () => {
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
        li.innerText,
        loadTimeData.getString('aiSubpageSublabelLoggingManagedDisabled'));
    assertFalse(!!li.querySelector('cr-icon'));
    assertTrue(!!li.querySelector('cr-policy-pref-indicator'));
  });
});
