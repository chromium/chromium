// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrIconButtonElement} from 'chrome://settings/lazy_load.js';
import type {SettingsSectionElement} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('SettingsSection', function() {
  let settingsSection: SettingsSectionElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsSection = document.createElement('settings-section');
    document.body.appendChild(settingsSection);
    flush();
  });

  test('Send feedback bubble', async function() {
    let feedback: CrIconButtonElement|null =
        settingsSection.shadowRoot!.querySelector('#feedback');
    assertFalse(!!feedback);

    settingsSection.showSendFeedbackButton = true;
    const feedbackClicked = eventToPromise('send-feedback', settingsSection);
    flush();

    feedback = settingsSection.shadowRoot!.querySelector('#feedback');
    assertTrue(!!feedback);
    feedback.click();
    await feedbackClicked;
  });
});