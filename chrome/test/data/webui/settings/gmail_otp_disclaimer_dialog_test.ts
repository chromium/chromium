// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsGmailOtpDisclaimerDialogElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('GmailOtpDisclaimerDialogTest', function() {
  let dialog: SettingsGmailOtpDisclaimerDialogElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      gmailOtpRequiredTitle:
          'To use this feature, first turn on smart features in Gmail',
      gmailOtpRequiredStep1:
          'Turn on “Smart features” in <a href="https://mail.google.com/mail/u/0/#settings/general" tabindex="0" aria-description="Opens in new tab" target="_blank">Gmail settings</a>',
      gmailOtpRequiredStep2:
          'Turn on “<a href="https://mail.google.com/mail/u/0/?ogwsfsd=true#settings" tabindex="0" aria-description="Opens in new tab" target="_blank">Smart features in other Google products</a>”',
      gotIt: 'Got it',
      close: 'Close',
    });

    dialog = document.createElement('settings-gmail-otp-disclaimer-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
  });

  test('UI components rendered correctly', function() {
    assertTrue(dialog.$.dialog.open);
    const title = dialog.shadowRoot!.querySelector('[slot=title]');
    assertTrue(!!title);
    assertEquals(
        'To use this feature, first turn on smart features in Gmail',
        title.textContent?.trim());

    const listItems = dialog.shadowRoot!.querySelectorAll('ol li');
    assertEquals(2, listItems.length);

    const step1 = listItems[0];
    assertTrue(!!step1);
    const step1Link = step1.querySelector('a');
    assertTrue(!!step1Link);
    assertEquals(
        'https://mail.google.com/mail/u/0/#settings/general',
        step1Link.getAttribute('href'));
    assertEquals('_blank', step1Link.getAttribute('target'));
    assertEquals('Gmail settings', step1Link.textContent?.trim());

    const step2 = listItems[1];
    assertTrue(!!step2);
    const step2Link = step2.querySelector('a');
    assertTrue(!!step2Link);
    assertEquals(
        'https://mail.google.com/mail/u/0/?ogwsfsd=true#settings',
        step2Link.getAttribute('href'));
    assertEquals('_blank', step2Link.getAttribute('target'));
    assertEquals(
        'Smart features in other Google products',
        step2Link.textContent?.trim());

    const confirmButton = dialog.$.confirmButton;
    assertTrue(isVisible(confirmButton));
    assertEquals('Got it', confirmButton.textContent?.trim());
  });

  test('Confirm button closes dialog', async function() {
    assertTrue(dialog.$.dialog.open);
    const closePromise = eventToPromise('close', dialog);
    dialog.$.confirmButton.click();
    await closePromise;
    assertFalse(dialog.$.dialog.open);
  });
});
