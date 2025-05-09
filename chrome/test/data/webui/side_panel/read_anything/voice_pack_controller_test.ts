// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, EXTENSION_RESPONSE_TIMEOUT_MS, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, SpeechBrowserProxyImpl, VoiceClientSideStatusCode, VoiceNotificationManager, VoicePackController, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceLanguageListener, VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

import {createAndSetVoices, createSpeechSynthesisVoice, setVoices} from './common.js';
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
  let installedLangs: string[];
  let uninstalledLangs: string[];
  let requestInfoLangs: string[];
  let notificationType: NotificationType|null;

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

  suite('restoreFromPrefs', () => {
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

    test('enables stored languages', () => {
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

      voicePackController.restoreFromPrefs();

      assertArrayEquals(
          [lang1, lang2, lang3], voicePackController.getEnabledLangs());
      assertEquals('en', voicePackController.getCurrentLanguage());
      assertTrue(voicePackController.isLangEnabled(lang1));
      assertTrue(voicePackController.isLangEnabled(lang2));
      assertTrue(voicePackController.isLangEnabled(lang3));
      assertTrue(onEnabledLangsChange);
    });

    test('enables the lang for the preferred voice', () => {
      chrome.readingMode.getStoredVoice = () => otherVoice.name;
      voicePackController.restoreFromPrefs();
      assertTrue(voicePackController.isLangEnabled(otherVoice.lang));
    });

    test('uses the stored voice for this language if there is one', () => {
      chrome.readingMode.getStoredVoice = () => otherVoice.name;

      voicePackController.restoreFromPrefs();

      assertTrue(onCurrentVoiceChange);
      assertEquals(otherVoice, voicePackController.getCurrentVoice());
    });

    test('uses the default voice if the stored voice is invalid', () => {
      chrome.readingMode.getStoredVoice = () => 'Matt';
      voicePackController.enableLang(langForDefaultVoice);

      voicePackController.restoreFromPrefs();

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

        voicePackController.restoreFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(
            defaultVoiceWithLang1, voicePackController.getCurrentVoice());
      });

      test('uses current voice if there\'s none for this language', () => {
        voicePackController.setCurrentLanguage(langWithNoVoices);
        voicePackController.setCurrentVoice(otherVoice);
        voicePackController.enableLang(otherVoice.lang);

        voicePackController.restoreFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(otherVoice, voicePackController.getCurrentVoice());
      });

      test('uses the device default if there\'s no current voice', () => {
        voicePackController.setCurrentLanguage(langWithNoVoices);
        voicePackController.enableLang(langForDefaultVoice);
        voicePackController.enableLang(otherVoice.lang);

        voicePackController.restoreFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(defaultVoice, voicePackController.getCurrentVoice());
      });

      test(
          'uses the first voice for this language if there\'s no default',
          () => {
            voicePackController.enableLang(lang2);
            voicePackController.setCurrentLanguage(lang2);

            voicePackController.restoreFromPrefs();

            assertTrue(onCurrentVoiceChange);
            assertEquals(
                firstVoiceWithLang2, voicePackController.getCurrentVoice());
          });
    });
  });

  test('onLanguageToggle enabled languages are added', () => {
    const firstLanguage = 'en-us';
    voicePackController.onLanguageToggle(firstLanguage);
    assertTrue(voicePackController.isLangEnabled(firstLanguage));
    assertTrue(
        chrome.readingMode.getLanguagesEnabledInPref().includes(firstLanguage));

    const secondLanguage = 'fr';
    voicePackController.onLanguageToggle(secondLanguage);
    assertTrue(voicePackController.isLangEnabled(secondLanguage));
    assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
        secondLanguage));
  });

  test('onLanguageToggle disabled languages are removed', () => {
    const firstLanguage = 'en-us';
    voicePackController.onLanguageToggle(firstLanguage);
    assertTrue(voicePackController.isLangEnabled(firstLanguage));
    assertTrue(
        chrome.readingMode.getLanguagesEnabledInPref().includes(firstLanguage));

    voicePackController.onLanguageToggle(firstLanguage);
    assertFalse(voicePackController.isLangEnabled(firstLanguage));
    assertFalse(
        chrome.readingMode.getLanguagesEnabledInPref().includes(firstLanguage));
  });

  test('onLanguageToggle with voice pack lang uninstalls it', () => {
    const lang = 'km';
    voicePackController.onLanguageToggle(lang);
    VoiceNotificationManager.getInstance().onVoiceStatusChange(
        lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);

    voicePackController.onLanguageToggle(lang);
    assertEquals(NotificationType.NONE, notificationType);
    assertArrayEquals([lang], uninstalledLangs);
  });

  test('onLanguageToggle with non voice pack lang does not uninstall', () => {
    const lang = 'zh';
    voicePackController.onLanguageToggle(lang);
    notificationType = null;

    voicePackController.onLanguageToggle(lang);

    assertFalse(!!notificationType);
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], uninstalledLangs);
    assertArrayEquals([], installedLangs);
  });

  test(
      'onLanguageToggle when previous language install failed, directly ' +
          'installs lang without sending status request first',
      () => {
        const lang = 'en-us';
        voicePackController.updateVoicePackStatus(lang, 'kOther');

        voicePackController.onLanguageToggle(lang);

        assertArrayEquals([lang], installedLangs);
        assertEquals(
            voicePackController.getLocalStatus(lang),
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY);
      });

  test(
      'onLanguageToggle when there is no status for lang, installs lang',
      () => {
        voicePackController.onLanguageToggle('en-us');
        assertArrayEquals(['en-us'], installedLangs);
      });


  test(
      'onLanguageToggle when language status is uninstalled, does not install',
      () => {
        const lang = 'en-us';
        voicePackController.updateVoicePackStatus(lang, 'kNotInstalled');

        voicePackController.onLanguageToggle(lang);

        assertArrayEquals([], installedLangs);
      });

  test(
      'onVoicesChanged with auto selected voice, switches to a Natural voice',
      () => {
        chrome.readingMode.getStoredVoice = () => '';
        const voice =
            createSpeechSynthesisVoice({lang: 'ja', name: 'Google Eagle'});
        const naturalVoice = createSpeechSynthesisVoice(
            {lang: 'ja', name: 'Google Horse (Natural)'});
        speech.setVoices([voice, naturalVoice]);
        voicePackController.setCurrentVoice(voice);
        voicePackController.setCurrentLanguage(voice.lang);

        voicePackController.onVoicesChanged();

        assertEquals(naturalVoice, voicePackController.getCurrentVoice());
      });

  test(
      'onVoicesChanged with a user selected voice, does not switch to a ' +
          'Natural voice',
      () => {
        const name = 'Google Emu';
        chrome.readingMode.getStoredVoice = () => name;
        const voice = createSpeechSynthesisVoice({lang: 'ja', name: name});
        const naturalVoice = createSpeechSynthesisVoice(
            {lang: 'ja', name: 'Google Ostrich (Natural)'});
        speech.setVoices([voice, naturalVoice]);
        voicePackController.setCurrentVoice(voice);
        voicePackController.setCurrentLanguage(voice.lang);

        voicePackController.onVoicesChanged();

        assertEquals(voice, voicePackController.getCurrentVoice());
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

  // <if expr="not is_chromeos">
  test('onVoicesChanged enables newly available langs', () => {
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
    voicePackController.restoreFromPrefs();
    assertArrayEquals([lang1], voicePackController.getEnabledLangs());
    assertArrayEquals([], chrome.readingMode.getLanguagesEnabledInPref());
    onEnabledLangsChange = false;

    speech.setVoices([
      createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Matt'}),
    ]);
    voicePackController.onVoicesChanged();

    // After voices come in, we should enable those langs.
    voicePackController.restoreFromPrefs();
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

  test('onVoicesChanged after new tts engine installs google locales', () => {
    const lang1 = 'bn-bd';
    const lang2 = 'hu-hu';
    const lang3 = 'en';
    voicePackController.enableLang(lang1);
    voicePackController.enableLang(lang2);
    voicePackController.enableLang(lang3);

    voicePackController.onTtsEngineInstalled();
    voicePackController.onVoicesChanged();

    assertArrayEquals(['bn', 'hu'], installedLangs);
    assertFalse(voicePackController.hasAvailableVoices());
  });

  test('onVoicesChanged restores from prefs on first voices received', () => {
    const lang = 'uk';
    const name = 'Google Lemur';
    const voice = createSpeechSynthesisVoice({lang, name});
    speech.setVoices([voice]);
    chrome.readingMode.getStoredVoice = () => name;

    voicePackController.onVoicesChanged();

    assertTrue(voicePackController.isLangEnabled(lang));
    assertArrayEquals([voice], voicePackController.getAvailableVoices());
  });

  test('onVoicesChanged requests info', () => {
    const lang1 = 'fi';
    const lang2 = 'id';
    const lang3 = 'da';
    voicePackController.setServerStatus(
        lang1, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
    voicePackController.setServerStatus(
        lang2, mojoVoicePackStatusToVoicePackStatusEnum('kAllocation'));
    voicePackController.setServerStatus(
        lang3, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));

    voicePackController.onVoicesChanged();

    assertArrayEquals([lang1, lang2, lang3], requestInfoLangs);
  });

  test('onVoicesChanged waits for engine timeout', () => {
    const lang = 'fi';
    voicePackController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
    const mockTimer = new MockTimer();
    mockTimer.install();

    voicePackController.onVoicesChanged();
    mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
    mockTimer.uninstall();

    assertEquals(NotificationType.GOOGLE_VOICES_UNAVAILABLE, notificationType);
  });

  test(
      'onVoicesChanged does nothing when current voice' +
          ' still available',
      () => {
        const voice = createSpeechSynthesisVoice({lang: 'id', name: 'Dog'});
        speech.setVoices([voice]);
        voicePackController.enableLang(voice.lang);
        voicePackController.setCurrentVoice(voice);
        onCurrentVoiceChange = false;

        voicePackController.onVoicesChanged();

        assertFalse(onCurrentVoiceChange);
        assertEquals(voice, voicePackController.getCurrentVoice());
      });

  test(
      'onVoicesChanged gets default voice when current' +
          ' voice unavailable',
      () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'id', name: 'Google Cat'});
        const defaultVoice =
            createSpeechSynthesisVoice({lang: 'id', name: 'Google Komodo'});
        speech.setVoices([defaultVoice]);
        voicePackController.enableLang(voice.lang);
        voicePackController.setCurrentVoice(voice);
        onCurrentVoiceChange = false;

        voicePackController.onVoicesChanged();

        assertTrue(onCurrentVoiceChange);
        assertEquals(defaultVoice, voicePackController.getCurrentVoice());
      });

  test('stopWaitingForSpeechExtension stops waiting for engine timeout', () => {
    const lang = 'fi';
    voicePackController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
    let notificationType = null;
    const notificationListener = {
      notify(type: NotificationType, _lang?: string): void {
        notificationType = type;
      },
    };
    VoiceNotificationManager.getInstance().addListener(notificationListener);
    const mockTimer = new MockTimer();
    mockTimer.install();

    voicePackController.onVoicesChanged();
    voicePackController.stopWaitingForSpeechExtension();
    mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
    mockTimer.uninstall();

    // Now download the voice since the speech engine responded.
    assertEquals(NotificationType.DOWNLOADING, notificationType);
  });

  test('onPageLanguageChanged updates current language', () => {
    const lang = 'el';
    chrome.readingMode.baseLanguageForSpeech = lang;

    voicePackController.onPageLanguageChanged();

    assertEquals(lang, voicePackController.getCurrentLanguage());
  });

  test('onPageLanguageChanged when not installed, requests info', () => {
    const lang = 'ja';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voicePackController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));


    voicePackController.onPageLanguageChanged();
    assertArrayEquals([lang], requestInfoLangs);

    voicePackController.updateVoicePackStatus(lang, 'kNotInstalled');
    assertEquals(
        VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
        voicePackController.getLocalStatus(lang));
    assertArrayEquals([lang], installedLangs);
  });

  test('onPageLanguageChanged when previously failed does not install', () => {
    const lang = 'ja';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voicePackController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));

    voicePackController.onPageLanguageChanged();

    assertFalse(!!voicePackController.getLocalStatus(lang));
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], installedLangs);
  });

  test('onPageLanguageChanged and already installing does not install', () => {
    const lang = 'ja';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voicePackController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalling'));

    voicePackController.onPageLanguageChanged();

    assertFalse(!!voicePackController.getLocalStatus(lang));
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], installedLangs);
  });

  test('onPageLanguageChanged and already installed does not install', () => {
    const lang = 'ja';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voicePackController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));

    voicePackController.onPageLanguageChanged();

    assertFalse(!!voicePackController.getLocalStatus(lang));
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], installedLangs);
  });

  suite('updateVoicePackStatus', () => {
    const lang = 'pt-br';

    setup(() => {
      voicePackController.enableLang(lang);
      chrome.readingMode.onLanguagePrefChange(lang, true);
    });

    test('with lang not marked for download does not install', () => {
      voicePackController.updateVoicePackStatus(lang, 'kNotInstalled');

      assertEquals(
          VoiceClientSideStatusCode.NOT_INSTALLED,
          voicePackController.getLocalStatus(lang));
      assertArrayEquals([], installedLangs);
    });

    test('with lang marked for download requests install', () => {
      chrome.readingMode.baseLanguageForSpeech = lang;
      voicePackController.onPageLanguageChanged();

      voicePackController.updateVoicePackStatus(lang, 'kNotInstalled');

      assertEquals(
          VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
          voicePackController.getLocalStatus(lang));
      const serverStatus = voicePackController.getServerStatus(lang);
      assertTrue(!!serverStatus);
      assertEquals(
          VoicePackServerStatusSuccessCode.NOT_INSTALLED, serverStatus.code);
      assertEquals('Successful response', serverStatus.id);
      assertArrayEquals([lang], installedLangs);
    });

    test('with no other voices for language, disables language', () => {
      setVoices(speech, []);

      voicePackController.updateVoicePackStatus(lang, 'kOther');

      assertFalse(voicePackController.isLangEnabled(lang));
      assertFalse(
          chrome.readingMode.getLanguagesEnabledInPref().includes(lang));
    });

    // <if expr="is_chromeos">
    test('chromeOS should disable if no google voices', () => {
      const lang1 = 'en-US';
      const lang2 = 'fr';
      const lang3 = 'yue';
      voicePackController.enableLang(lang1);
      chrome.readingMode.onLanguagePrefChange(lang1.toLowerCase(), true);
      voicePackController.enableLang(lang2);
      chrome.readingMode.onLanguagePrefChange(lang2, true);
      voicePackController.enableLang(lang3);
      chrome.readingMode.onLanguagePrefChange(lang3, true);
      createAndSetVoices(speech, [
        {lang: lang1, name: 'Henry'},
        {lang: lang2, name: 'Google Thomas'},
      ]);

      voicePackController.updateVoicePackStatus(lang1, 'kOther');
      voicePackController.updateVoicePackStatus(lang2, 'kOther');
      voicePackController.updateVoicePackStatus(lang3, 'kOther');

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
    test('desktop should only disable if no voices at all', () => {
      const lang1 = 'en-US';
      const lang2 = 'fr';
      const lang3 = 'yue';
      voicePackController.enableLang(lang1);
      chrome.readingMode.onLanguagePrefChange(lang1.toLowerCase(), true);
      voicePackController.enableLang(lang2);
      chrome.readingMode.onLanguagePrefChange(lang2, true);
      voicePackController.enableLang(lang3);
      chrome.readingMode.onLanguagePrefChange(lang3, true);
      createAndSetVoices(speech, [
        {lang: lang1, name: 'Henry'},
        {lang: lang2, name: 'Google Thomas'},
      ]);
      onEnabledLangsChange = false;

      voicePackController.updateVoicePackStatus(lang1, 'kOther');
      voicePackController.updateVoicePackStatus(lang2, 'kOther');
      voicePackController.updateVoicePackStatus(lang3, 'kOther');

      const langsInPrefs = chrome.readingMode.getLanguagesEnabledInPref();
      assertTrue(langsInPrefs.includes(lang1.toLowerCase()));
      assertTrue(langsInPrefs.includes(lang2));
      assertFalse(langsInPrefs.includes(lang3), 'lang3 prefs');
      assertTrue(voicePackController.isLangEnabled(lang1));
      assertTrue(voicePackController.isLangEnabled(lang2));
      assertFalse(voicePackController.isLangEnabled(lang3), 'lang3');
    });
    // </if>

    test(
        'when language-pack lang does not match voice lang, still disables it',
        () => {
          voicePackController.enableLang('it-it');
          setVoices(speech, []);

          voicePackController.updateVoicePackStatus('it', 'kOther');

          assertFalse(voicePackController.isLangEnabled('it-it'), 'controller');
          assertFalse(
              chrome.readingMode.getLanguagesEnabledInPref().includes('it-it'),
              'prefs');
        });

    test(
        'when language-pack lang does not match voice lang, with ' +
            'e-speak voices, still disables language',
        () => {
          voicePackController.enableLang('it-it');
          createAndSetVoices(speech, [
            {lang: 'it', name: 'eSpeak Italian '},
          ]);

          voicePackController.updateVoicePackStatus('it', 'kOther');

          assertFalse(voicePackController.isLangEnabled('it-it'));
          assertFalse(
              chrome.readingMode.getLanguagesEnabledInPref().includes('it-it'));
        });

    test(
        'and has other Google voices for language, keeps language enabled',
        () => {
          createAndSetVoices(speech, [
            {lang: lang, name: 'Google Portuguese 1'},
            {lang: lang, name: 'Google Portuguese 2'},
          ]);
          voicePackController.updateVoicePackStatus(lang, 'kOther');

          assertTrue(voicePackController.isLangEnabled(lang), 'controller');
          assertTrue(
              chrome.readingMode.getLanguagesEnabledInPref().includes(lang),
              'prefs');
        });

    test(
        'unavailable if natural voices are in the list for a different lang',
        () => {
          const lang = 'fr';
          createAndSetVoices(speech, [
            {lang: 'it', name: 'Google Chicken (Natural)'},
          ]);

          voicePackController.updateVoicePackStatus(lang, 'kInstalled');

          const serverStatus = voicePackController.getServerStatus(lang);
          assertTrue(!!serverStatus, 'status is: ' + serverStatus?.id);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voicePackController.getLocalStatus(lang));
        });

    test(
        'unavailable if system voices are in the list for a different lang',
        () => {
          const lang = 'de';

          // Installed 'de' language pack, but the fake available voice list
          // only has english voices.
          voicePackController.updateVoicePackStatus(lang, 'kInstalled');

          const serverStatus = voicePackController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voicePackController.getLocalStatus(lang));
        });

    test(
        'unavailable if only system voices are in the list for this lang',
        () => {
          const lang = 'en';

          voicePackController.updateVoicePackStatus(lang, 'kInstalled');

          const serverStatus = voicePackController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voicePackController.getLocalStatus(lang));
        });

    test(
        'available if natural voices are unsupported for this lang and voices' +
            ' are available',
        () => {
          const lang = 'yue';
          createAndSetVoices(speech, [
            {lang: 'yue-hk', name: 'Cantonese'},
          ]);

          voicePackController.updateVoicePackStatus(lang, 'kInstalled');

          const serverStatus = voicePackController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.AVAILABLE,
              voicePackController.getLocalStatus(lang));
        });

    test(
        'unavailable if natural voices are unsupported for this lang and ' +
            'voices unavailable',
        () => {
          const lang = 'yue';

          voicePackController.updateVoicePackStatus(lang, 'kInstalled');

          const serverStatus = voicePackController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voicePackController.getLocalStatus(lang));
        });

    test('available if natural voices are installed for this lang', () => {
      const lang = 'en-us';
      // set installing status so that the old status is not empty.
      voicePackController.updateVoicePackStatus(lang, 'kInstalling');
      // set the voices on speech synthesis without triggering on voices
      // changed, so we can verify that updateVoicePackStatus calls it.
      createAndSetVoices(speech, [
        {lang: lang, name: 'Wall-e (Natural)'},
        {lang: lang, name: 'Andy (Natural)'},
      ]);
      voicePackController.updateVoicePackStatus(lang, 'kInstalled');

      const serverStatus = voicePackController.getServerStatus(lang);
      assertTrue(!!serverStatus);
      assertEquals(
          VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
      assertEquals('Successful response', serverStatus.id);
      // This would be INSTALLED_AND_UNAVIALABLE if the voice list wasn't
      // refreshed.
      assertEquals(
          VoiceClientSideStatusCode.AVAILABLE,
          voicePackController.getLocalStatus(lang));
    });

    test(
        'switches to newly available voices if it\'s for the current language',
        () => {
          const lang = 'en-us';
          chrome.readingMode.baseLanguageForSpeech = lang;
          voicePackController.enableLang(lang);
          chrome.readingMode.getStoredVoice = () => '';
          createAndSetVoices(
              speech, [{lang: lang, name: 'Google Cow (Natural)'}]);
          voicePackController.updateVoicePackStatus(lang, 'kInstalled');

          const selectedVoice = voicePackController.getCurrentVoice();
          assertTrue(!!selectedVoice);
          assertEquals(lang, selectedVoice.lang);
          assertTrue(selectedVoice.name.includes('Natural'));
        });

    test(
        'does not switch to newly available voices if it\'s not for the ' +
            'current language',
        () => {
          const installedLang = 'en-us';
          chrome.readingMode.baseLanguageForSpeech = 'pt-br';
          voicePackController.enableLang(
              chrome.readingMode.baseLanguageForSpeech);
          const currentVoice = createSpeechSynthesisVoice({
            name: 'Portuguese voice 1',
            lang: chrome.readingMode.baseLanguageForSpeech,
          });
          voicePackController.setCurrentVoice(currentVoice);
          chrome.readingMode.getStoredVoice = () => '';
          setVoices(speech, [currentVoice]);

          voicePackController.updateVoicePackStatus(
              installedLang, 'kInstalled');

          // The selected voice should stay the same as it was.
          assertEquals(currentVoice, voicePackController.getCurrentVoice());
        });

    test('with error code marks the status', () => {
      const lang = 'en-us';

      voicePackController.updateVoicePackStatus(lang, 'kOther');

      const serverStatus = voicePackController.getServerStatus(lang);
      assertTrue(!!serverStatus);
      assertEquals(serverStatus.code, VoicePackServerStatusErrorCode.OTHER);
      assertEquals('Unsuccessful response', serverStatus.id);
      assertEquals(
          voicePackController.getLocalStatus(lang),
          VoiceClientSideStatusCode.ERROR_INSTALLING);
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
