// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoiceLanguageModel} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('VoiceLanguageModel', () => {
  let voiceLanguageModel: VoiceLanguageModel;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    voiceLanguageModel = new VoiceLanguageModel();
  });

  test('enableLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.enableLang(lang1);
    voiceLanguageModel.enableLang(lang2);
    voiceLanguageModel.enableLang(lang3);

    assertEquals(3, voiceLanguageModel.getEnabledLangs().size);
    assertTrue(voiceLanguageModel.getEnabledLangs().has(lang1));
    assertTrue(voiceLanguageModel.getEnabledLangs().has(lang2));
    assertTrue(voiceLanguageModel.getEnabledLangs().has(lang3));
  });

  test('disableLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.enableLang(lang1);
    voiceLanguageModel.enableLang(lang2);
    voiceLanguageModel.enableLang(lang3);
    voiceLanguageModel.disableLang(lang1);
    voiceLanguageModel.disableLang(lang2);
    voiceLanguageModel.disableLang('random');

    assertEquals(1, voiceLanguageModel.getEnabledLangs().size);
    assertTrue(voiceLanguageModel.getEnabledLangs().has(lang3));
  });

  test('setAvailableLangs', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.setAvailableLangs([lang1, lang2, lang3, lang3]);

    assertEquals(3, voiceLanguageModel.getAvailableLangs().size);
    assertTrue(voiceLanguageModel.getAvailableLangs().has(lang1));
    assertTrue(voiceLanguageModel.getAvailableLangs().has(lang2));
    assertTrue(voiceLanguageModel.getAvailableLangs().has(lang3));
  });

  test('setAvailableVoices', () => {
    const voice1 = createSpeechSynthesisVoice({lang: 'tr', name: 'Jane'});
    const voice2 = createSpeechSynthesisVoice({lang: 'it-it', name: 'Kat'});
    const voice3 = createSpeechSynthesisVoice({lang: 'pr', name: 'Anne'});

    voiceLanguageModel.setAvailableVoices([voice1, voice2, voice3]);

    assertArrayEquals(
        [voice1, voice2, voice3], voiceLanguageModel.getAvailableVoices());
  });

  // <if expr="not is_chromeos">
  test('addPossiblyDisabledLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.addPossiblyDisabledLang(lang1);
    voiceLanguageModel.addPossiblyDisabledLang(lang2);
    voiceLanguageModel.addPossiblyDisabledLang(lang3);

    assertEquals(3, voiceLanguageModel.getPossiblyDisabledLangs().size);
    assertTrue(voiceLanguageModel.getPossiblyDisabledLangs().has(lang1));
    assertTrue(voiceLanguageModel.getPossiblyDisabledLangs().has(lang2));
    assertTrue(voiceLanguageModel.getPossiblyDisabledLangs().has(lang3));
  });

  test('removePossiblyDisabledLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.addPossiblyDisabledLang(lang1);
    voiceLanguageModel.addPossiblyDisabledLang(lang2);
    voiceLanguageModel.addPossiblyDisabledLang(lang3);
    voiceLanguageModel.removePossiblyDisabledLang(lang1);
    voiceLanguageModel.removePossiblyDisabledLang(lang2);

    assertEquals(1, voiceLanguageModel.getPossiblyDisabledLangs().size);
    assertTrue(voiceLanguageModel.getPossiblyDisabledLangs().has(lang3));
  });
  // </if>

  test('setServerStatus', () => {
    const lang1 = 'abc';
    const lang2 = 'it-it';
    const lang3 = 'en-us';
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalling');
    const status3 = mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot');

    voiceLanguageModel.setServerStatus(lang1, status1);
    voiceLanguageModel.setServerStatus(lang2, status2);
    voiceLanguageModel.setServerStatus(lang3, status3);

    assertEquals(status1, voiceLanguageModel.getServerStatus(lang1));
    assertEquals(status2, voiceLanguageModel.getServerStatus(lang2));
    assertEquals(status3, voiceLanguageModel.getServerStatus(lang3));
  });

  test('setServerStatus on same language overrides', () => {
    const lang = 'abc';
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalling');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kOther');

    voiceLanguageModel.setServerStatus(lang, status1);
    voiceLanguageModel.setServerStatus(lang, status2);

    assertEquals(status2, voiceLanguageModel.getServerStatus(lang));
  });

  test('getServerLanguages', () => {
    const lang1 = 'abc';
    const lang2 = 'it-it';
    const lang3 = 'en-us';
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalling');
    const status3 = mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot');

    voiceLanguageModel.setServerStatus(lang1, status1);
    voiceLanguageModel.setServerStatus(lang2, status2);
    voiceLanguageModel.setServerStatus(lang3, status3);

    assertArrayEquals(
        [lang1, lang2, lang3], voiceLanguageModel.getServerLanguages());
  });

  test('setLocalStatus', () => {
    const lang1 = 'abc';
    const lang2 = 'it-it';
    const lang3 = 'en-us';
    const status1 = VoiceClientSideStatusCode.NOT_INSTALLED;
    const status2 = VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE;
    const status3 = VoiceClientSideStatusCode.ERROR_INSTALLING;

    voiceLanguageModel.setLocalStatus(lang1, status1);
    voiceLanguageModel.setLocalStatus(lang2, status2);
    voiceLanguageModel.setLocalStatus(lang3, status3);

    assertEquals(status1, voiceLanguageModel.getLocalStatus(lang1));
    assertEquals(status2, voiceLanguageModel.getLocalStatus(lang2));
    assertEquals(status3, voiceLanguageModel.getLocalStatus(lang3));
  });

  test('setLocalStatus on same language overrides', () => {
    const lang = 'abc';
    const status1 = VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE;
    const status2 = VoiceClientSideStatusCode.AVAILABLE;

    voiceLanguageModel.setLocalStatus(lang, status1);
    voiceLanguageModel.setLocalStatus(lang, status2);

    assertEquals(status2, voiceLanguageModel.getLocalStatus(lang));
  });

  test('addLanguageForDownload', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.addLanguageForDownload(lang1);
    voiceLanguageModel.addLanguageForDownload(lang2);
    voiceLanguageModel.addLanguageForDownload(lang3);

    assertTrue(voiceLanguageModel.hasLanguageForDownload(lang1));
    assertTrue(voiceLanguageModel.hasLanguageForDownload(lang2));
    assertTrue(voiceLanguageModel.hasLanguageForDownload(lang3));
  });

  test('removeLanguageForDownload', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voiceLanguageModel.addLanguageForDownload(lang1);
    voiceLanguageModel.addLanguageForDownload(lang2);
    voiceLanguageModel.addLanguageForDownload(lang3);
    voiceLanguageModel.removeLanguageForDownload(lang1);
    voiceLanguageModel.removeLanguageForDownload(lang2);

    assertFalse(voiceLanguageModel.hasLanguageForDownload(lang1));
    assertFalse(voiceLanguageModel.hasLanguageForDownload(lang2));
    assertTrue(voiceLanguageModel.hasLanguageForDownload(lang3));
  });
});
