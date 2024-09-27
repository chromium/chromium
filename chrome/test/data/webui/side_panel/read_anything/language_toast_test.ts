// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LanguageToastElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {NotificationType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

suite('LanguageToast', () => {
  let toast: LanguageToastElement;

  function getTitle(): string {
    return toast.$.toast.querySelector<HTMLElement>('#toastTitle')!.textContent!
        ;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toast = document.createElement('language-toast');
    document.body.appendChild(toast);
    return microtasksFinished();
  });

  // <if expr="chromeos_ash">
  test('shows downloaded message on ChromeOS', async () => {
    const lang = 'pt-br';
    toast.notify(lang, NotificationType.DOWNLOADING);
    toast.notify(lang, NotificationType.DOWNLOADED);
    await microtasksFinished();

    assertTrue(toast.$.toast.open);
    assertEquals(
        'High-quality Português (Brasil) voice files downloaded', getTitle());
  });

  test('does not show toast if not newly downloaded', async () => {
    const lang = 'pt-br';
    toast.notify(lang, NotificationType.DOWNLOADED);
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });
  // </if>

  // <if expr="not is_chromeos">
  test('no downloaded message outside ChromeOS', async () => {
    const lang = 'pt-br';
    toast.notify(lang, NotificationType.DOWNLOADING);
    toast.notify(lang, NotificationType.DOWNLOADED);
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });
  // </if>

  test('shows error message if enabled', async () => {
    const lang = 'pt-br';
    toast.showErrors = true;
    toast.notify(lang, NotificationType.NO_SPACE_HQ);
    await microtasksFinished();

    assertTrue(toast.$.toast.open);
    assertEquals(
        'For higher quality voices, clear space on your device', getTitle());
  });

  test('no error message if disabled', async () => {
    const lang = 'pt-br';
    toast.showErrors = false;
    toast.notify(lang, NotificationType.NO_SPACE_HQ);
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });
});
