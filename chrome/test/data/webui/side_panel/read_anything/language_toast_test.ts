// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LanguageToastElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {NotificationType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice} from './common.js';

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
    toast.availableVoices = [];
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

  test('shows error for no internet and no voices', async () => {
    const lang = 'pt-br';
    toast.showErrors = true;
    toast.notify(lang, NotificationType.NO_INTERNET);
    await microtasksFinished();

    assertTrue(toast.$.toast.open);
    assertEquals('Can\'t use Read Aloud right now.', getTitle());
  });

  test('no error for no internet with some voices', async () => {
    const lang = 'pt-br';
    toast.availableVoices =
        [createSpeechSynthesisVoice({name: 'Odium', lang: lang})];
    toast.showErrors = true;
    toast.notify(lang, NotificationType.NO_INTERNET);
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });

  test(
      'no error for no internet with some voices in a different lang',
      async () => {
        const lang = 'pt-br';
        toast.availableVoices =
            [createSpeechSynthesisVoice({name: 'Edolyn', lang: 'en-au'})];
        toast.showErrors = true;
        toast.notify(lang, NotificationType.NO_INTERNET);
        await microtasksFinished();

        assertFalse(toast.$.toast.open);
      });

  test('shows error for no space and no voices', async () => {
    const lang = 'pt-br';
    toast.showErrors = true;
    toast.notify(lang, NotificationType.NO_SPACE);
    await microtasksFinished();

    assertTrue(toast.$.toast.open);
    assertEquals('To use Read Aloud, clear space on your device', getTitle());
  });

  test('no error for no space with some voices', async () => {
    const lang = 'pt-br';
    toast.availableVoices =
        [createSpeechSynthesisVoice({name: 'Odium', lang: lang})];
    toast.showErrors = true;
    toast.notify(lang, NotificationType.NO_SPACE);
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });

  test(
      'no error for no space with some voices in a different lang',
      async () => {
        const lang = 'pt-br';
        toast.availableVoices =
            [createSpeechSynthesisVoice({name: 'Edolyn', lang: 'en-au'})];
        toast.showErrors = true;
        toast.notify(lang, NotificationType.NO_SPACE);
        await microtasksFinished();

        assertFalse(toast.$.toast.open);
      });
});
