// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {BrowserProxy, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LanguageChanged', () => {
  let app: AppElement;
  let voiceLanguageController: VoiceLanguageController;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
    app = await createApp();
  });

  test('updates toolbar fonts', async () => {
    let updatedFontsOnToolbar = false;
    app.$.toolbar.updateFonts = () => {
      updatedFontsOnToolbar = true;
    };

    app.languageChanged();
    await microtasksFinished();

    assertTrue(updatedFontsOnToolbar);
  });

  test('sets current language', () => {
    const lang1 = 'vi';
    const lang2 = 'zh';
    const lang3 = 'tr';

    chrome.readingMode.baseLanguageForSpeech = lang1;
    app.languageChanged();
    assertEquals(lang1, voiceLanguageController.getCurrentLanguage());

    chrome.readingMode.baseLanguageForSpeech = lang2;
    app.languageChanged();
    assertEquals(lang2, voiceLanguageController.getCurrentLanguage());

    chrome.readingMode.baseLanguageForSpeech = lang3;
    app.languageChanged();
    assertEquals(lang3, voiceLanguageController.getCurrentLanguage());
  });
});
