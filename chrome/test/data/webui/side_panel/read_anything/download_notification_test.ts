// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, SpeechBrowserProxyImpl, ToolbarEvent, VoiceClientSideStatusCode, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, LanguageToastElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createAndSetVoices, createApp, emitEvent} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('Download notification', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let voiceLanguageController: VoiceLanguageController;

  const lang = 'en-us';
  let toast: LanguageToastElement;

  function installLanguage(): Promise<void> {
    setNaturalVoicesForLang(lang);
    // existing status
    voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
    // then we request install
    voiceLanguageController.setLocalStatus(
        lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
    voiceLanguageController.updateLanguageStatus(lang, 'kInstalling');
    // install completes
    voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');
    return microtasksFinished();
  }

  function setNaturalVoicesForLang(lang: string) {
    createAndSetVoices(speech, [
      {lang: lang, name: 'Wall-e (Natural)'},
      {lang: lang, name: 'Andy (Natural)'},
      {lang: lang, name: 'Buzz'},
    ]);
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
    app = await createApp();

    toast = app.$.languageToast;
  });

  test('does not show if already installed', async () => {
    // The first call to update status should be the existing status from
    // the server.
    voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });

  test('does not show if still installing', async () => {
    // existing status
    voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
    // then we request install
    voiceLanguageController.updateLanguageStatus(lang, 'kInstalling');
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });

  test('does not show if error while installing', async () => {
    // existing status
    voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
    // then we request install
    voiceLanguageController.updateLanguageStatus(lang, 'kInstalling');
    // install error
    voiceLanguageController.updateLanguageStatus(lang, 'kOther');
    await microtasksFinished();

    assertFalse(toast.$.toast.open);
  });

  test('shows after installed', async () => {
    await installLanguage();
    assertTrue(toast.$.toast.open);
  });

  test('does not show with language menu open', async () => {
    emitEvent(app, ToolbarEvent.LANGUAGE_MENU_OPEN);
    await installLanguage();
    assertFalse(toast.$.toast.open);
  });

  test('shows again after language menu close', async () => {
    emitEvent(app, ToolbarEvent.LANGUAGE_MENU_OPEN);
    await installLanguage();

    emitEvent(app, ToolbarEvent.LANGUAGE_MENU_CLOSE);
    await installLanguage();
    assertTrue(toast.$.toast.open);
  });
});
