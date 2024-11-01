// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'chrome://settings/settings.js';

import type {SettingsAiLoggingInfoBullet} from 'chrome://settings/settings.js';
import {loadTimeData, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    const crIcon = li.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(crIcon.icon, 'settings20:account-box');
  });

  test('infoBulletManaged', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING,
    };
    await createRow(pref);

    const li = row.shadowRoot!.querySelector('li');
    assertTrue(!!li);
    assertEquals(
        li.innerText,
        loadTimeData.getString('aiSubpageSublabelLoggingManagedDisabled'));
    const crIcon = li.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(crIcon.icon, loadTimeData.getString('managedByIcon'));
  });
});
