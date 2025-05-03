// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoicePackModel} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('VoicePackModel', () => {
  let voicePackModel: VoicePackModel;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    voicePackModel = new VoicePackModel();
  });

  test('enableLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.enableLang(lang1);
    voicePackModel.enableLang(lang2);
    voicePackModel.enableLang(lang3);

    assertEquals(3, voicePackModel.getEnabledLangs().size);
    assertTrue(voicePackModel.getEnabledLangs().has(lang1));
    assertTrue(voicePackModel.getEnabledLangs().has(lang2));
    assertTrue(voicePackModel.getEnabledLangs().has(lang3));
  });

  test('disableLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.enableLang(lang1);
    voicePackModel.enableLang(lang2);
    voicePackModel.enableLang(lang3);
    assertTrue(voicePackModel.disableLang(lang1));
    assertTrue(voicePackModel.disableLang(lang2));
    assertFalse(voicePackModel.disableLang('random'));

    assertEquals(1, voicePackModel.getEnabledLangs().size);
    assertTrue(voicePackModel.getEnabledLangs().has(lang3));
  });

  test('setAvailableLangs', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.setAvailableLangs([lang1, lang2, lang3, lang3]);

    assertEquals(3, voicePackModel.getAvailableLangs().size);
    assertTrue(voicePackModel.getAvailableLangs().has(lang1));
    assertTrue(voicePackModel.getAvailableLangs().has(lang2));
    assertTrue(voicePackModel.getAvailableLangs().has(lang3));
  });

  test('setAvailableVoices', () => {
    const voice1 = createSpeechSynthesisVoice({lang: 'tr', name: 'Jane'});
    const voice2 = createSpeechSynthesisVoice({lang: 'it-it', name: 'Kat'});
    const voice3 = createSpeechSynthesisVoice({lang: 'pr', name: 'Anne'});

    voicePackModel.setAvailableVoices([voice1, voice2, voice3]);

    assertArrayEquals(
        [voice1, voice2, voice3], voicePackModel.getAvailableVoices());
  });

  // <if expr="not is_chromeos">
  test('addPossiblyDisabledLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.addPossiblyDisabledLang(lang1);
    voicePackModel.addPossiblyDisabledLang(lang2);
    voicePackModel.addPossiblyDisabledLang(lang3);

    assertEquals(3, voicePackModel.getPossiblyDisabledLangs().size);
    assertTrue(voicePackModel.getPossiblyDisabledLangs().has(lang1));
    assertTrue(voicePackModel.getPossiblyDisabledLangs().has(lang2));
    assertTrue(voicePackModel.getPossiblyDisabledLangs().has(lang3));
  });

  test('removePossiblyDisabledLang', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.addPossiblyDisabledLang(lang1);
    voicePackModel.addPossiblyDisabledLang(lang2);
    voicePackModel.addPossiblyDisabledLang(lang3);
    voicePackModel.removePossiblyDisabledLang(lang1);
    voicePackModel.removePossiblyDisabledLang(lang2);

    assertEquals(1, voicePackModel.getPossiblyDisabledLangs().size);
    assertTrue(voicePackModel.getPossiblyDisabledLangs().has(lang3));
  });
  // </if>

  test('setServerStatus', () => {
    const lang1 = 'abc';
    const lang2 = 'it-it';
    const lang3 = 'en-us';
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalling');
    const status3 = mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot');

    voicePackModel.setServerStatus(lang1, status1);
    voicePackModel.setServerStatus(lang2, status2);
    voicePackModel.setServerStatus(lang3, status3);

    assertEquals(status1, voicePackModel.getServerStatus(lang1));
    assertEquals(status2, voicePackModel.getServerStatus(lang2));
    assertEquals(status3, voicePackModel.getServerStatus(lang3));
  });

  test('setServerStatus on same language overrides', () => {
    const lang = 'abc';
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalling');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kOther');

    voicePackModel.setServerStatus(lang, status1);
    voicePackModel.setServerStatus(lang, status2);

    assertEquals(status2, voicePackModel.getServerStatus(lang));
  });

  test('getServerLanguages', () => {
    const lang1 = 'abc';
    const lang2 = 'it-it';
    const lang3 = 'en-us';
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalling');
    const status3 = mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot');

    voicePackModel.setServerStatus(lang1, status1);
    voicePackModel.setServerStatus(lang2, status2);
    voicePackModel.setServerStatus(lang3, status3);

    assertArrayEquals(
        [lang1, lang2, lang3], voicePackModel.getServerLanguages());
  });

  test('setLocalStatus', () => {
    const lang1 = 'abc';
    const lang2 = 'it-it';
    const lang3 = 'en-us';
    const status1 = VoiceClientSideStatusCode.NOT_INSTALLED;
    const status2 = VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE;
    const status3 = VoiceClientSideStatusCode.ERROR_INSTALLING;

    voicePackModel.setLocalStatus(lang1, status1);
    voicePackModel.setLocalStatus(lang2, status2);
    voicePackModel.setLocalStatus(lang3, status3);

    assertEquals(status1, voicePackModel.getLocalStatus(lang1));
    assertEquals(status2, voicePackModel.getLocalStatus(lang2));
    assertEquals(status3, voicePackModel.getLocalStatus(lang3));
  });

  test('setLocalStatus on same language overrides', () => {
    const lang = 'abc';
    const status1 = VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE;
    const status2 = VoiceClientSideStatusCode.AVAILABLE;

    voicePackModel.setLocalStatus(lang, status1);
    voicePackModel.setLocalStatus(lang, status2);

    assertEquals(status2, voicePackModel.getLocalStatus(lang));
  });

  test('addLanguageForDownload', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.addLanguageForDownload(lang1);
    voicePackModel.addLanguageForDownload(lang2);
    voicePackModel.addLanguageForDownload(lang3);

    assertTrue(voicePackModel.hasLanguageForDownload(lang1));
    assertTrue(voicePackModel.hasLanguageForDownload(lang2));
    assertTrue(voicePackModel.hasLanguageForDownload(lang3));
  });

  test('removeLanguageForDownload', () => {
    const lang1 = 'de';
    const lang2 = 'hi';
    const lang3 = 'xyz';

    voicePackModel.addLanguageForDownload(lang1);
    voicePackModel.addLanguageForDownload(lang2);
    voicePackModel.addLanguageForDownload(lang3);
    voicePackModel.removeLanguageForDownload(lang1);
    voicePackModel.removeLanguageForDownload(lang2);

    assertFalse(voicePackModel.hasLanguageForDownload(lang1));
    assertFalse(voicePackModel.hasLanguageForDownload(lang2));
    assertTrue(voicePackModel.hasLanguageForDownload(lang3));
  });
});
