// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoicePackModel} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

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
});
