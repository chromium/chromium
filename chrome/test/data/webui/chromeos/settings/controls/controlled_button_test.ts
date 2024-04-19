// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://os-settings/os_settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ControlledButtonElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {clearBody} from '../utils.js';

// clang-format on

suite('controlled button', function() {
  let controlledButton: ControlledButtonElement;

  const uncontrolledPref: chrome.settingsPrivate.PrefObject = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
  };

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const extensionControlledPref = Object.assign(
      {
        controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      },
      uncontrolledPref);

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const policyControlledPref = Object.assign(
      {
        controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      },
      uncontrolledPref);

  function queryCrButton() {
    return controlledButton.shadowRoot!.querySelector('cr-button')!;
  }

  setup(function() {
    clearBody();
    controlledButton = document.createElement('controlled-button');
    controlledButton.pref = uncontrolledPref;
    document.body.appendChild(controlledButton);
    flush();
  });

  test('controlled prefs', function() {
    assertFalse(queryCrButton().disabled);
    assertFalse(!!controlledButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));

    controlledButton.pref = extensionControlledPref;
    flush();
    assertTrue(queryCrButton().disabled);
    assertTrue(!!controlledButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));

    controlledButton.pref = policyControlledPref;
    flush();
    assertTrue(queryCrButton().disabled);
    const indicator =
        controlledButton.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);
    assertGT(indicator!.clientHeight, 0);

    controlledButton.pref = uncontrolledPref;
    flush();
    assertFalse(queryCrButton().disabled);
    assertFalse(!!controlledButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
  });

  test('null pref', function() {
    controlledButton.pref = extensionControlledPref;
    flush();
    assertTrue(queryCrButton().disabled);
    assertTrue(!!controlledButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));

    controlledButton.pref = undefined;
    flush();
    assertFalse(queryCrButton().disabled);
    assertFalse(!!controlledButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
  });

  test('action-button', function() {
    assertNotEquals('action-button', queryCrButton().className);

    const controlledActionButton = document.createElement('controlled-button');
    controlledActionButton.pref = uncontrolledPref;
    controlledActionButton.className = 'action-button';
    document.body.appendChild(controlledActionButton);
    flush();
    assertEquals(
        'action-button',
        controlledActionButton.shadowRoot!.querySelector(
                                              'cr-button')!.className);
  });
});
