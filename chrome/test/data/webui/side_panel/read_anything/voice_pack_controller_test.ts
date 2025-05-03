// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {NotificationType, VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {BrowserProxy, mojoVoicePackStatusToVoicePackStatusEnum, SpeechBrowserProxyImpl, VoiceClientSideStatusCode, VoiceNotificationManager, VoicePackController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('VoicePackController', () => {
  let speech: TestSpeechBrowserProxy;
  let voicePackController: VoicePackController;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    voicePackController = new VoicePackController();
  });

  suite('setLocalStatus', () => {
    let listener: VoiceNotificationListener;
    let listenerNotified: boolean;

    setup(() => {
      listenerNotified = false;
      listener = {
        notify(_type: NotificationType, _language: string): void {
          listenerNotified = true;
        },
      };
      VoiceNotificationManager.setInstance(new VoiceNotificationManager());
      VoiceNotificationManager.getInstance().addListener(listener);
    });

    test('no notification for non-Google language', () => {
      voicePackController.setLocalStatus(
          'zh', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for invalid language', () => {
      voicePackController.setLocalStatus(
          'klingon', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for same status', () => {
      voicePackController.setLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
      listenerNotified = false;

      voicePackController.setLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);

      assertFalse(listenerNotified);
    });

    test('notifies for new status with Google-supported language', () => {
      voicePackController.setLocalStatus(
          'it-it', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);

      listenerNotified = false;
      voicePackController.setLocalStatus(
          'it-it', VoiceClientSideStatusCode.AVAILABLE);
      assertTrue(listenerNotified);

      listenerNotified = false;
      voicePackController.setLocalStatus(
          'hi', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
    });
  });

  test('setServerStatus uses voice pack lang', () => {
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kOther');

    voicePackController.setServerStatus('de-de', status1);
    voicePackController.setServerStatus('yue-hk', status2);

    assertEquals(status1, voicePackController.getServerStatus('de-de'));
    assertEquals(status2, voicePackController.getServerStatus('yue-hk'));
    assertEquals(status1, voicePackController.getServerStatus('de'));
    assertEquals(status2, voicePackController.getServerStatus('yue'));
  });

  test('disableLang', () => {
    assertFalse(voicePackController.disableLang('no'));
    assertFalse(voicePackController.disableLang(''));

    voicePackController.enableLang('vi');
    assertTrue(voicePackController.disableLang('vi'));
    assertFalse(voicePackController.isLangEnabled('vi'));
    assertFalse(voicePackController.isLangEnabled('VI'));
  });

  test('enableLang', () => {
    assertFalse(voicePackController.enableLang(''));
    assertTrue(voicePackController.enableLang('no'));
    assertFalse(voicePackController.enableLang('no'));
    assertTrue(voicePackController.isLangEnabled('no'));
    assertTrue(voicePackController.isLangEnabled('NO'));
  });

  test('getInitialListOfEnabledLanguages', () => {
    const lang1 = 'en-gb';
    const lang2 = 'fr';
    const lang3 = 'bd';
    chrome.readingMode.onLanguagePrefChange(lang1, true);
    chrome.readingMode.onLanguagePrefChange(lang2, true);
    chrome.readingMode.onLanguagePrefChange(lang3, true);
    chrome.readingMode.baseLanguageForSpeech = 'en';
    speech.setVoices([
      createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Matt'}),
    ]);
    voicePackController.refreshAvailableVoices();

    assertArrayEquals(
        [lang1, lang2, lang3],
        voicePackController.getInitialListOfEnabledLanguages());

    assertTrue(voicePackController.isLangEnabled(lang1));
    assertTrue(voicePackController.isLangEnabled(lang2));
    assertTrue(voicePackController.isLangEnabled(lang3));
  });

  // <if expr="is_chromeos">
  test(
      'disableLangIfNoVoices chromeOS should disable if no google voices',
      () => {
        const lang1 = 'en-US';
        const lang2 = 'fr';
        const lang3 = 'yue';
        voicePackController.enableLang(lang1);
        chrome.readingMode.onLanguagePrefChange(lang1.toLowerCase(), true);
        voicePackController.enableLang(lang2);
        chrome.readingMode.onLanguagePrefChange(lang2, true);
        voicePackController.enableLang(lang3);
        chrome.readingMode.onLanguagePrefChange(lang3, true);
        speech.setVoices([
          createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
          createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
        ]);

        assertTrue(voicePackController.disableLangIfNoVoices(lang1));
        assertFalse(voicePackController.disableLangIfNoVoices(lang2));
        assertTrue(voicePackController.disableLangIfNoVoices(lang3));

        const langsInPrefs = chrome.readingMode.getLanguagesEnabledInPref();
        assertFalse(langsInPrefs.includes(lang1.toLowerCase()));
        assertTrue(langsInPrefs.includes(lang2));
        assertFalse(langsInPrefs.includes(lang3));
        assertFalse(voicePackController.isLangEnabled(lang1));
        assertTrue(voicePackController.isLangEnabled(lang2));
        assertFalse(voicePackController.isLangEnabled(lang3));
      });
  // </if>

  // <if expr="not is_chromeos">
  test(
      'disableLangIfNoVoices desktop should only disable if no voices at all',
      () => {
        const lang1 = 'en-US';
        const lang2 = 'fr';
        const lang3 = 'yue';
        voicePackController.enableLang(lang1);
        chrome.readingMode.onLanguagePrefChange(lang1.toLowerCase(), true);
        voicePackController.enableLang(lang2);
        chrome.readingMode.onLanguagePrefChange(lang2, true);
        voicePackController.enableLang(lang3);
        chrome.readingMode.onLanguagePrefChange(lang3, true);
        speech.setVoices([
          createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
          createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
        ]);

        assertFalse(voicePackController.disableLangIfNoVoices(lang1));
        assertFalse(voicePackController.disableLangIfNoVoices(lang2));
        assertTrue(voicePackController.disableLangIfNoVoices(lang3));

        const langsInPrefs = chrome.readingMode.getLanguagesEnabledInPref();
        assertTrue(langsInPrefs.includes(lang1.toLowerCase()));
        assertTrue(langsInPrefs.includes(lang2));
        assertFalse(langsInPrefs.includes(lang3));
        assertTrue(voicePackController.isLangEnabled(lang1));
        assertTrue(voicePackController.isLangEnabled(lang2));
        assertFalse(voicePackController.isLangEnabled(lang3));
      });

  test('enableNowAvailableLangs', () => {
    const lang1 = 'en-gb';
    const lang2 = 'fr';
    const lang3 = 'bd';
    chrome.readingMode.onLanguagePrefChange(lang1, true);
    chrome.readingMode.onLanguagePrefChange(lang2, true);
    chrome.readingMode.onLanguagePrefChange(lang3, true);
    chrome.readingMode.baseLanguageForSpeech = 'en';
    speech.setVoices([
      createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
    ]);
    voicePackController.refreshAvailableVoices();
    assertArrayEquals(
        [lang1], voicePackController.getInitialListOfEnabledLanguages());
    assertArrayEquals([], chrome.readingMode.getLanguagesEnabledInPref());

    speech.setVoices([
      createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Matt'}),
    ]);
    voicePackController.refreshAvailableVoices(true);
    voicePackController.enableNowAvailableLangs();

    // After voices come in, we should enable those langs.
    assertArrayEquals(
        [lang1, lang2, lang3],
        voicePackController.getInitialListOfEnabledLanguages());
    assertArrayEquals(
        [lang1, lang2, lang3], chrome.readingMode.getLanguagesEnabledInPref());
    assertTrue(voicePackController.isLangEnabled(lang1));
    assertTrue(voicePackController.isLangEnabled(lang2));
    assertTrue(voicePackController.isLangEnabled(lang3));
  });
  // </if>
});
