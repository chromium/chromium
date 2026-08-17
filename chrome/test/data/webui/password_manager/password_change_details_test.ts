// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {PasswordChangeDetailsElement, PrefToggleButtonElement} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {makePasswordManagerPrefs} from './test_util.js';

suite('PasswordChangeDetailsTest', function() {
  let element: PasswordChangeDetailsElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('renders pref-toggle-button when flag is enabled', function() {
    loadTimeData.overrideValues({
      isPasswordChangeWithPrivateInferenceLoginCheckEnabled: true,
    });
    element = document.createElement('password-change-details');
    element.prefs = makePasswordManagerPrefs();
    document.body.appendChild(element);
    flush();

    const toggle = element.shadowRoot!.querySelector<PrefToggleButtonElement>(
        '#passwordChangeToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);
    assertTrue(toggle.pref.value);
    const fallbackDiv = element.shadowRoot!.querySelector('.cr-row.first');
    assertFalse(!!fallbackDiv);
  });

  test('pref-toggle-button reflects disabled pref value', function() {
    loadTimeData.overrideValues({
      isPasswordChangeWithPrivateInferenceLoginCheckEnabled: true,
    });
    element = document.createElement('password-change-details');
    const prefs = makePasswordManagerPrefs();
    prefs.automated_password_change_enabled.value = false;
    element.prefs = prefs;
    document.body.appendChild(element);
    flush();

    const toggle = element.shadowRoot!.querySelector<PrefToggleButtonElement>(
        '#passwordChangeToggle');
    assertTrue(!!toggle);
    assertFalse(toggle.checked);
    assertFalse(toggle.pref.value);
  });

  test('renders fallback description when flag is disabled', function() {
    loadTimeData.overrideValues({
      isPasswordChangeWithPrivateInferenceLoginCheckEnabled: false,
    });
    element = document.createElement('password-change-details');
    element.prefs = makePasswordManagerPrefs();
    document.body.appendChild(element);
    flush();

    const toggle = element.shadowRoot!.querySelector('#passwordChangeToggle');
    assertFalse(!!toggle);
    const fallbackDiv = element.shadowRoot!.querySelector('.cr-row.first');
    assertTrue(!!fallbackDiv);
  });
});
