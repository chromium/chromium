// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, EXTENSION_RESPONSE_TIMEOUT_MS, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, SpeechBrowserProxyImpl, VoiceClientSideStatusCode, VoiceNotificationManager, VoicePackController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceLanguageListener, VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('VoicePackController', () => {
  let speech: TestSpeechBrowserProxy;
  let voicePackController: VoicePackController;
  let listener: VoiceLanguageListener;
  let onEnabledLangsChange: boolean;
  let onAvailableVoicesChange: boolean;
  let onCurrentVoiceChange: boolean;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    voicePackController = new VoicePackController();
    onEnabledLangsChange = false;
    onAvailableVoicesChange = false;
    onCurrentVoiceChange = false;
    listener = {
      onEnabledLangsChange() {
        onEnabledLangsChange = true;
      },
      onAvailableVoicesChange() {
        onAvailableVoicesChange = true;
      },
      onCurrentVoiceChange() {
        onCurrentVoiceChange = true;
      },
    };
    voicePackController.addListener(listener);
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
    voicePackController.enableLang('vi');
    onEnabledLangsChange = false;

    voicePackController.disableLang('');
    assertFalse(onEnabledLangsChange);
    voicePackController.disableLang('no');
    assertFalse(onEnabledLangsChange);

    voicePackController.disableLang('vi');
    assertTrue(onEnabledLangsChange);
    assertFalse(voicePackController.isLangEnabled('vi'));
    assertFalse(voicePackController.isLangEnabled('VI'));
  });

  test('enableLang', () => {
    voicePackController.enableLang('');
    assertFalse(onEnabledLangsChange);

    voicePackController.enableLang('no');
    assertTrue(onEnabledLangsChange);

    onEnabledLangsChange = false;
    voicePackController.enableLang('no');
    assertFalse(onEnabledLangsChange);
    assertTrue(voicePackController.isLangEnabled('no'));
    assertTrue(voicePackController.isLangEnabled('NO'));
  });

  test('setCurrentVoice', () => {
    const voice = createSpeechSynthesisVoice({lang: 'tr', name: 'Zebra'});
    voicePackController.setCurrentVoice(voice);
    assertTrue(onCurrentVoiceChange);
    assertEquals(voice, voicePackController.getCurrentVoice());

    onCurrentVoiceChange = false;
    voicePackController.setCurrentVoice(voice);
    assertFalse(onCurrentVoiceChange);
    assertEquals(voice, voicePackController.getCurrentVoice());
  });

  test('setUserPreferredVoice', () => {
    let sentVoiceName = '';
    let sentLang = '';
    chrome.readingMode.onVoiceChange = (name, lang) => {
      sentVoiceName = name;
      sentLang = lang;
    };
    const voice = createSpeechSynthesisVoice({lang: 'tr', name: 'Lion'});

    voicePackController.setUserPreferredVoice(voice);

    assertTrue(onCurrentVoiceChange);
    assertEquals(voice, voicePackController.getCurrentVoice());
    assertEquals(voice.name, sentVoiceName);
    assertEquals(voice.lang, sentLang);
  });

  suite('setUserPreferredVoiceFromPrefs', () => {
    const langForDefaultVoice = 'en';
    const lang1 = 'zh';
    const lang2 = 'tr';
    const langWithNoVoices = 'elvish';

    const defaultVoice = createSpeechSynthesisVoice({
      lang: langForDefaultVoice,
      name: 'Google Kristi',
      default: true,
    });
    const firstVoiceWithLang1 =
        createSpeechSynthesisVoice({lang: lang1, name: 'Google Monkey'});
    const defaultVoiceWithLang1 = createSpeechSynthesisVoice({
      lang: lang1,
      name: 'Google Llama',
      default: true,
    });
    const firstVoiceWithLang2 =
        createSpeechSynthesisVoice({lang: lang2, name: 'Google Parrot'});
    const secondVoiceWithLang2 =
        createSpeechSynthesisVoice({lang: lang2, name: 'Google Panda'});
    const otherVoice =
        createSpeechSynthesisVoice({lang: 'it', name: 'Google Elephant'});
    const voices = [
      defaultVoice,
      firstVoiceWithLang1,
      defaultVoiceWithLang1,
      otherVoice,
      firstVoiceWithLang2,
      secondVoiceWithLang2,
    ];

    setup(() => {
      speech.setVoices(voices);
    });

    test('enables the lang for the chosen voice', () => {
      chrome.readingMode.getStoredVoice = () => otherVoice.name;
      voicePackController.setUserPreferredVoiceFromPrefs();
      assertTrue(voicePackController.isLangEnabled(otherVoice.lang));
    });

    test('uses the stored voice for this language if there is one', () => {
      chrome.readingMode.getStoredVoice = () => otherVoice.name;

      voicePackController.setUserPreferredVoiceFromPrefs();

      assertTrue(onCurrentVoiceChange);
      assertEquals(otherVoice, voicePackController.getCurrentVoice());
    });

    test('uses the default voice if the stored voice is invalid', () => {
      chrome.readingMode.getStoredVoice = () => 'Matt';
      voicePackController.enableLang(langForDefaultVoice);

      voicePackController.setUserPreferredVoiceFromPrefs();

      assertTrue(onCurrentVoiceChange);
      assertEquals(defaultVoice, voicePackController.getCurrentVoice());
    });

    suite('when there is no stored voice for this language', () => {
      setup(() => {
        chrome.readingMode.getStoredVoice = () => '';
      });

      test('uses the default voice for this language', () => {
        voicePackController.enableLang(lang1);
        voicePackController.setCurrentLanguage(lang1);

        voicePackController.setUserPreferredVoiceFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(
            defaultVoiceWithLang1, voicePackController.getCurrentVoice());
      });

      test('uses current voice if there\'s none for this language', () => {
        voicePackController.setCurrentLanguage(langWithNoVoices);
        voicePackController.setCurrentVoice(otherVoice);
        voicePackController.enableLang(otherVoice.lang);

        voicePackController.setUserPreferredVoiceFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(otherVoice, voicePackController.getCurrentVoice());
      });

      test('uses the device default if there\'s no current voice', () => {
        voicePackController.setCurrentLanguage(langWithNoVoices);
        voicePackController.enableLang(langForDefaultVoice);
        voicePackController.enableLang(otherVoice.lang);

        voicePackController.setUserPreferredVoiceFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(defaultVoice, voicePackController.getCurrentVoice());
      });

      test(
          'uses the first voice for this language if there\'s no default',
          () => {
            voicePackController.enableLang(lang2);
            voicePackController.setCurrentLanguage(lang2);

            voicePackController.setUserPreferredVoiceFromPrefs();

            assertTrue(onCurrentVoiceChange);
            assertEquals(
                firstVoiceWithLang2, voicePackController.getCurrentVoice());
          });
    });
  });

  test(
      'updateAutoSelectedVoiceToNaturalVoice with auto selected voice, ' +
          'switches to a Natural voice',
      () => {
        chrome.readingMode.getStoredVoice = () => '';
        const voice = createSpeechSynthesisVoice({lang: 'ja', name: 'Eagle'});
        const naturalVoice =
            createSpeechSynthesisVoice({lang: 'ja', name: 'Horse (Natural)'});
        voicePackController.setCurrentVoice(voice);
        voicePackController.setCurrentLanguage(voice.lang);
        voicePackController.setAvailableVoices([voice, naturalVoice]);

        voicePackController.updateAutoSelectedVoiceToNaturalVoice();

        assertEquals(naturalVoice, voicePackController.getCurrentVoice());
      });

  test(
      'updateAutoSelectedVoiceToNaturalVoice with a user selected voice, does' +
          ' not switch to a Natural voice',
      () => {
        const name = 'Emu';
        chrome.readingMode.getStoredVoice = () => name;
        const voice = createSpeechSynthesisVoice({lang: 'ja', name: name});
        const naturalVoice =
            createSpeechSynthesisVoice({lang: 'ja', name: 'Ostrich (Natural)'});
        voicePackController.setCurrentVoice(voice);
        voicePackController.setCurrentLanguage(voice.lang);
        voicePackController.setAvailableVoices([voice, naturalVoice]);

        voicePackController.updateAutoSelectedVoiceToNaturalVoice();

        assertEquals(voice, voicePackController.getCurrentVoice());
      });

  test('restoreEnabledLanguagesFromPref', () => {
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

    voicePackController.restoreEnabledLanguagesFromPref();

    assertArrayEquals(
        [lang1, lang2, lang3], voicePackController.getEnabledLangs());
    assertEquals('en', voicePackController.getCurrentLanguage());
    assertTrue(voicePackController.isLangEnabled(lang1));
    assertTrue(voicePackController.isLangEnabled(lang2));
    assertTrue(voicePackController.isLangEnabled(lang3));
    assertTrue(onEnabledLangsChange);
  });

  test('refreshAvailableVoices', () => {
    const voices = [
      createSpeechSynthesisVoice({lang: 'en', name: 'Google Henry'}),
      createSpeechSynthesisVoice({lang: 'en', name: 'Google Thomas'}),
      createSpeechSynthesisVoice({lang: 'en', name: 'Google Matt'}),
    ];
    speech.setVoices(voices);

    assertFalse(onAvailableVoicesChange);
    assertArrayEquals([], voicePackController.getAvailableVoices());

    voicePackController.refreshAvailableVoices();
    assertTrue(onAvailableVoicesChange);
    assertArrayEquals(voices, voicePackController.getAvailableVoices());

    // If we already have voices and new voices come in, we only get those
    // voices when we force a refresh.
    onAvailableVoicesChange = false;
    const newVoices = voices.concat(
        createSpeechSynthesisVoice({lang: 'it', name: 'Google Charles'}));
    speech.setVoices(newVoices);

    voicePackController.refreshAvailableVoices();
    assertFalse(onAvailableVoicesChange);
    assertArrayEquals(voices, voicePackController.getAvailableVoices());

    voicePackController.refreshAvailableVoices(true);
    assertTrue(onAvailableVoicesChange);
    assertArrayEquals(newVoices, voicePackController.getAvailableVoices());
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
        onEnabledLangsChange = false;

        voicePackController.disableLangIfNoVoices(lang1);
        assertTrue(onEnabledLangsChange);

        onEnabledLangsChange = false;
        voicePackController.disableLangIfNoVoices(lang2);
        assertFalse(onEnabledLangsChange);

        voicePackController.disableLangIfNoVoices(lang3);
        assertTrue(onEnabledLangsChange);

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
        onEnabledLangsChange = false;

        voicePackController.disableLangIfNoVoices(lang1);
        assertFalse(onEnabledLangsChange);
        voicePackController.disableLangIfNoVoices(lang2);
        assertFalse(onEnabledLangsChange);
        voicePackController.disableLangIfNoVoices(lang3);
        assertTrue(onEnabledLangsChange);

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
    voicePackController.restoreEnabledLanguagesFromPref();
    assertArrayEquals([lang1], voicePackController.getEnabledLangs());
    assertArrayEquals([], chrome.readingMode.getLanguagesEnabledInPref());
    onEnabledLangsChange = false;

    speech.setVoices([
      createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Matt'}),
    ]);
    voicePackController.refreshAvailableVoices(true);
    voicePackController.enableNowAvailableLangs();

    // After voices come in, we should enable those langs.
    voicePackController.restoreEnabledLanguagesFromPref();
    assertArrayEquals(
        [lang1, lang2, lang3], voicePackController.getEnabledLangs());
    assertArrayEquals(
        [lang1, lang2, lang3], chrome.readingMode.getLanguagesEnabledInPref());
    assertTrue(voicePackController.isLangEnabled(lang1));
    assertTrue(voicePackController.isLangEnabled(lang2));
    assertTrue(voicePackController.isLangEnabled(lang3));
    assertTrue(onEnabledLangsChange);
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

    test('updateUnavailableVoiceToDefaultVoice requests info', () => {
      const lang1 = 'fi';
      const lang2 = 'id';
      const lang3 = 'da';
      voicePackController.setServerStatus(
          lang1, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
      voicePackController.setServerStatus(
          lang2, mojoVoicePackStatusToVoicePackStatusEnum('kAllocation'));
      voicePackController.setServerStatus(
          lang3, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));

      voicePackController.updateUnavailableVoiceToDefaultVoice();

      assertArrayEquals([lang1, lang2, lang3], requestInfoLangs);
    });

    test(
        'updateUnavailableVoiceToDefaultVoice waits for engine timeout', () => {
          const lang = 'fi';
          voicePackController.setServerStatus(
              lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
          const mockTimer = new MockTimer();
          mockTimer.install();

          voicePackController.updateUnavailableVoiceToDefaultVoice();
          mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
          mockTimer.uninstall();

          assertEquals(
              NotificationType.GOOGLE_VOICES_UNAVAILABLE, notificationType);
        });

    test(
        'updateUnavailableVoiceToDefaultVoice does nothing when current voice' +
            ' still available',
        () => {
          const voice = createSpeechSynthesisVoice({lang: 'id', name: 'Dog'});
          voicePackController.enableLang(voice.lang);
          voicePackController.setCurrentVoice(voice);
          voicePackController.setAvailableVoices([voice]);
          onCurrentVoiceChange = false;

          voicePackController.updateUnavailableVoiceToDefaultVoice();

          assertFalse(onCurrentVoiceChange);
          assertEquals(voice, voicePackController.getCurrentVoice());
        });

    test(
        'updateUnavailableVoiceToDefaultVoice gets default voice when current' +
            ' voice unavailable',
        () => {
          const voice = createSpeechSynthesisVoice({lang: 'id', name: 'Cat'});
          const defaultVoice =
              createSpeechSynthesisVoice({lang: 'id', name: 'Komodo'});
          voicePackController.enableLang(voice.lang);
          voicePackController.setCurrentVoice(voice);
          voicePackController.setAvailableVoices([defaultVoice]);
          onCurrentVoiceChange = false;

          voicePackController.updateUnavailableVoiceToDefaultVoice();

          assertTrue(onCurrentVoiceChange);
          assertEquals(defaultVoice, voicePackController.getCurrentVoice());
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

          voicePackController.updateUnavailableVoiceToDefaultVoice();
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

  test('voiceUnavailable selects default voice', () => {
    const voice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Google Giraffe'});
    speech.setVoices([voice]);

    voicePackController.onVoiceUnavailableError();

    assertEquals(voice, voicePackController.getCurrentVoice());
  });

  test(
      'voiceUnavailable default voice is current voice, selects another voice',
      () => {
        const voice1 =
            createSpeechSynthesisVoice({lang: 'en', name: 'Google George'});
        const voice2 =
            createSpeechSynthesisVoice({lang: 'en', name: 'Google Connie'});
        voicePackController.setCurrentVoice(voice1);
        speech.setVoices([voice1, voice2]);

        voicePackController.onVoiceUnavailableError();

        assertEquals(voice2, voicePackController.getCurrentVoice());
      });

  test(
      'voiceUnavailable continues to select default voice if no voices ' +
          'available in language',
      () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'en', name: 'Google Penguin'});
        speech.setVoices([voice]);

        voicePackController.onVoiceUnavailableError();

        assertEquals(voice, voicePackController.getCurrentVoice());
      });
});
