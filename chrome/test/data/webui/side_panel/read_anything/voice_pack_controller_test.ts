// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, EXTENSION_RESPONSE_TIMEOUT_MS, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, SpeechBrowserProxyImpl, VoiceClientSideStatusCode, VoiceNotificationManager, VoicePackController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

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

  suite('installation', () => {
    let installedLangs: string[];
    let uninstalledLangs: string[];
    let requestInfoLangs: string[];
    let notificationType: NotificationType|null;

    setup(() => {
      installedLangs = [];
      uninstalledLangs = [];
      requestInfoLangs = [];
      chrome.readingMode.sendGetVoicePackInfoRequest = (lang) => {
        requestInfoLangs.push(lang);
      };
      chrome.readingMode.sendInstallVoicePackRequest = (lang) => {
        installedLangs.push(lang);
      };
      chrome.readingMode.sendUninstallVoiceRequest = (lang) => {
        uninstalledLangs.push(lang);
      };
      const notificationListener = {
        notify(type: NotificationType, _lang?: string): void {
          notificationType = type;
        },
      };
      VoiceNotificationManager.getInstance().addListener(notificationListener);
    });

    test('refreshVoicePackStatuses with no languages does nothing', () => {
      voicePackController.refreshVoicePackStatuses();

      assertArrayEquals([], requestInfoLangs);
    });

    test('refreshVoicePackStatuses with languages requests info', () => {
      const lang1 = 'fi';
      const lang2 = 'id';
      const lang3 = 'da';
      voicePackController.setServerStatus(
          lang1, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
      voicePackController.setServerStatus(
          lang2, mojoVoicePackStatusToVoicePackStatusEnum('kAllocation'));
      voicePackController.setServerStatus(
          lang3, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));

      voicePackController.refreshVoicePackStatuses();

      assertArrayEquals([lang1, lang2, lang3], requestInfoLangs);
    });

    test(
        'refreshVoicePackStatuses with languages waits for engine timeout',
        () => {
          const lang = 'fi';
          voicePackController.setServerStatus(
              lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
          const mockTimer = new MockTimer();
          mockTimer.install();

          voicePackController.refreshVoicePackStatuses();
          mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
          mockTimer.uninstall();

          assertEquals(
              NotificationType.GOOGLE_VOICES_UNAVAILABLE, notificationType);
        });

    test(
        'stopWaitingForSpeechExtension stops waiting for engine timeout',
        () => {
          const lang = 'fi';
          voicePackController.setServerStatus(
              lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
          let notificationType = null;
          const notificationListener = {
            notify(type: NotificationType, _lang?: string): void {
              notificationType = type;
            },
          };
          VoiceNotificationManager.getInstance().addListener(
              notificationListener);
          const mockTimer = new MockTimer();
          mockTimer.install();

          voicePackController.refreshVoicePackStatuses();
          voicePackController.stopWaitingForSpeechExtension();
          mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
          mockTimer.uninstall();

          assertFalse(!!notificationType);
        });

    test(
        'triggerInstall with lang not marked for download does nothing', () => {
          const lang = 'es-es';

          voicePackController.triggerInstall(lang);

          assertEquals(
              VoiceClientSideStatusCode.NOT_INSTALLED,
              voicePackController.getLocalStatus(lang));
          assertArrayEquals([], installedLangs);
        });

    test(
        'triggerInstall with lang marked for download requests install', () => {
          const lang = 'fil';

          assertTrue(voicePackController.requestInstall(lang, false));
          voicePackController.triggerInstall(lang);

          assertEquals(
              VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
              voicePackController.getLocalStatus(lang));
          assertArrayEquals([lang], installedLangs);
        });

    test('requestInstall with no status, requests install on retry', () => {
      const lang = 'ja';

      assertTrue(voicePackController.requestInstall(lang, true));

      assertEquals(
          VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
          voicePackController.getLocalStatus(lang));
      assertArrayEquals([lang], installedLangs);
    });

    test('requestInstall when not installed, requests info', () => {
      const lang = 'ja';
      voicePackController.setServerStatus(
          lang, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));

      assertTrue(voicePackController.requestInstall(lang, true));
      assertArrayEquals([lang], requestInfoLangs);

      voicePackController.triggerInstall(lang);
      assertEquals(
          VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
          voicePackController.getLocalStatus(lang));
      assertArrayEquals([lang], installedLangs);
    });

    test(
        'requestInstall when previously failed and should retry, retries',
        () => {
          const lang = 'ja';
          voicePackController.setServerStatus(
              lang, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));

          assertTrue(voicePackController.requestInstall(lang, true));

          assertEquals(
              VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY,
              voicePackController.getLocalStatus(lang));
          assertArrayEquals([lang], installedLangs);
        });

    test(
        'requestInstall when previously failed and should not retry does' +
            'nothing',
        () => {
          const lang = 'ja';
          voicePackController.setServerStatus(
              lang, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));

          assertFalse(voicePackController.requestInstall(lang, false));

          assertFalse(!!voicePackController.getLocalStatus(lang));
          assertArrayEquals([], requestInfoLangs);
          assertArrayEquals([], installedLangs);
        });

    test('requestInstall and already installing does nothing', () => {
      const lang = 'ja';
      voicePackController.setServerStatus(
          lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalling'));

      assertFalse(voicePackController.requestInstall(lang, false));

      assertFalse(!!voicePackController.getLocalStatus(lang));
      assertArrayEquals([], requestInfoLangs);
      assertArrayEquals([], installedLangs);
    });

    test('requestInstall and already installed does nothing', () => {
      const lang = 'ja';
      voicePackController.setServerStatus(
          lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));

      assertFalse(voicePackController.requestInstall(lang, false));

      assertFalse(!!voicePackController.getLocalStatus(lang));
      assertArrayEquals([], requestInfoLangs);
      assertArrayEquals([], installedLangs);
    });

    test('uninstall with voice pack lang uninstalls', () => {
      const lang = 'km';
      voicePackController.requestInstall(lang, false);
      VoiceNotificationManager.getInstance().onVoiceStatusChange(
          lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);

      voicePackController.uninstall(lang);
      assertEquals(NotificationType.NONE, notificationType);
      assertArrayEquals([lang], requestInfoLangs);
      assertArrayEquals([lang], uninstalledLangs);

      voicePackController.triggerInstall(lang);
      assertEquals(
          VoiceClientSideStatusCode.NOT_INSTALLED,
          voicePackController.getLocalStatus(lang));
      assertArrayEquals([], installedLangs);
    });

    test('uninstall with non voice pack lang does nothing', () => {
      const lang = 'zh';
      voicePackController.requestInstall(lang, false);
      notificationType = null;

      voicePackController.uninstall(lang);

      assertFalse(!!notificationType);
      assertArrayEquals([], requestInfoLangs);
      assertArrayEquals([], uninstalledLangs);
      assertArrayEquals([], installedLangs);
    });
  });
});
