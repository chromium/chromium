// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ControlledRadioButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('controlled radio button', function() {
  let radioButton: ControlledRadioButtonElement;

  const pref: chrome.settingsPrivate.PrefObject = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioButton = document.createElement('controlled-radio-button');
    radioButton.set('pref', pref);
    document.body.appendChild(radioButton);
  });

  test('disables when pref is managed', function() {
    radioButton.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    flush();
    assertTrue(radioButton.disabled);
    assertFalse(
        !!radioButton.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    radioButton.set('name', 'true');
    flush();
    assertTrue(
        !!radioButton.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    // See https://github.com/Polymer/polymer/issues/4652#issuecomment-305471987
    // on why |null| must be used here instead of |undefined|.
    radioButton.set('pref.enforcement', null);
    flush();
    assertFalse(radioButton.disabled);
    assertEquals(
        'none',
        radioButton.shadowRoot!.querySelector(
                                   'cr-policy-pref-indicator')!.style.display);
  });
});
