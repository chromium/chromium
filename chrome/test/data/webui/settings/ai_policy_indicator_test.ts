// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsAiPolicyIndicator} from 'chrome://settings/lazy_load.js';
import {ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

// clang-format on

type PrefObject = chrome.settingsPrivate.PrefObject;

suite('PolicyIndicator', function() {
  let row: SettingsAiPolicyIndicator;

  function createRow(pref: PrefObject) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    row = document.createElement('settings-ai-policy-indicator');
    row.pref = pref;
    document.body.appendChild(row);
    return flushTasks();
  }

  test('indicatorVisible', async () => {
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.DISABLE,
    };
    await createRow(pref);

    const indicator = row.shadowRoot!.querySelector('#aiPolicyIndicator');
    assertTrue(!!indicator);
    assertTrue(isVisible(indicator));
    assertTrue(!!indicator.querySelector('cr-policy-pref-indicator'));
  });

  test('indicatorHidden', async () => {
    // Case 1: Allowed.
    const pref: PrefObject = {
      key: 'some_ai_feature_enterprise_pref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    };
    await createRow(pref);

    let indicator = row.shadowRoot!.querySelector('#aiPolicyIndicator');
    assertFalse(!!indicator);

    // Case 2: Allowed without logging.
    pref.value = ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING;
    await createRow(pref);

    indicator = row.shadowRoot!.querySelector('#aiPolicyIndicator');
    assertFalse(!!indicator);
  });
});
