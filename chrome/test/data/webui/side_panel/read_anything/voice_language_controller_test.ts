// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {AVAILABLE_GOOGLE_TTS_LOCALES, BrowserProxy, EXTENSION_RESPONSE_TIMEOUT_MS, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, SpeechBrowserProxyImpl, VoiceClientSideStatusCode, VoiceLanguageController, VoiceNotificationManager, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceLanguageListener, VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

import {createAndSetVoices, createSpeechSynthesisVoice, setupBasicSpeech, setVoices} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('VoiceLanguageController', () => {
  let speech: TestSpeechBrowserProxy;
  let voiceLanguageController: VoiceLanguageController;
  let listener: VoiceLanguageListener;
  let onEnabledLangsChange: boolean;
  let onAvailableVoicesChange: boolean;
  let onCurrentVoiceChange: boolean;
  let installedLangs: string[];
  let uninstalledLangs: string[];
  let requestInfoLangs: string[];
  let notificationType: NotificationType|null;

  const langForDefaultVoice = 'en';
  const lang1 = 'zh';
  const lang2 = 'tr';
  const lang3 = 'pt-br';
  const langWithNoVoices = 'elvish';

  const defaultVoice = createSpeechSynthesisVoice({
    lang: langForDefaultVoice,
    name: 'Google Penny',
    default: true,
  });
  const firstVoiceWithLang1 =
      createSpeechSynthesisVoice({lang: lang1, name: 'Google Nickel'});
  const defaultVoiceWithLang1 = createSpeechSynthesisVoice(
      {lang: lang1, name: 'Google Dime', default: true});
  const firstVoiceWithLang2 =
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Quarter'});
  const secondVoiceWithLang2 =
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Dollar'});
  const firstVoiceWithLang3 =
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Penny'});
  const naturalVoiceWithLang3 =
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Penny (Natural)'});
  const otherVoice =
      createSpeechSynthesisVoice({lang: 'it', name: 'Google Bill'});
  const voices = [
    defaultVoice,
    firstVoiceWithLang1,
    defaultVoiceWithLang1,
    otherVoice,
    firstVoiceWithLang2,
    secondVoiceWithLang2,
    firstVoiceWithLang3,
    naturalVoiceWithLang3,
  ];

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    voiceLanguageController = new VoiceLanguageController();
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
    voiceLanguageController.addListener(listener);
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
    notificationType = null;
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
      voiceLanguageController.setLocalStatus(
          'zh', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for invalid language', () => {
      voiceLanguageController.setLocalStatus(
          'klingon', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for same status', () => {
      voiceLanguageController.setLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
      listenerNotified = false;

      voiceLanguageController.setLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);

      assertFalse(listenerNotified);
    });

    test('notifies for new status with Google-supported language', () => {
      voiceLanguageController.setLocalStatus(
          'it-it', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);

      listenerNotified = false;
      voiceLanguageController.setLocalStatus(
          'it-it', VoiceClientSideStatusCode.AVAILABLE);
      assertTrue(listenerNotified);

      listenerNotified = false;
      voiceLanguageController.setLocalStatus(
          'hi', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
    });
  });

  test('setServerStatus uses voice pack lang', () => {
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kOther');

    voiceLanguageController.setServerStatus('de-de', status1);
    voiceLanguageController.setServerStatus('yue-hk', status2);

    assertEquals(status1, voiceLanguageController.getServerStatus('de-de'));
    assertEquals(status2, voiceLanguageController.getServerStatus('yue-hk'));
    assertEquals(status1, voiceLanguageController.getServerStatus('de'));
    assertEquals(status2, voiceLanguageController.getServerStatus('yue'));
  });

  test('enableLang', () => {
    voiceLanguageController.enableLang('');
    assertFalse(onEnabledLangsChange);

    voiceLanguageController.enableLang('no');
    assertTrue(onEnabledLangsChange);

    onEnabledLangsChange = false;
    voiceLanguageController.enableLang('no');
    assertFalse(onEnabledLangsChange);
    assertTrue(voiceLanguageController.isLangEnabled('no'));
    assertTrue(voiceLanguageController.isLangEnabled('NO'));
  });

  test('setUserPreferredVoice', () => {
    let sentVoiceName = '';
    let sentLang = '';
    chrome.readingMode.onVoiceChange = (name, lang) => {
      sentVoiceName = name;
      sentLang = lang;
    };
    const voice = createSpeechSynthesisVoice({lang: 'tr', name: 'Lion'});

    voiceLanguageController.setUserPreferredVoice(voice);

    assertTrue(onCurrentVoiceChange);
    assertEquals(voice, voiceLanguageController.getCurrentVoice());
    assertEquals(voice.name, sentVoiceName);
    assertEquals(voice.lang, sentLang);
  });

  test('restoreFromPrefs removes unavailable languages from prefs', () => {
    const previouslyAvailableLang = 'pt-pt';
    chrome.readingMode.onLanguagePrefChange(previouslyAvailableLang, true);
    setupBasicSpeech(speech);

    voiceLanguageController.restoreFromPrefs();

    assertArrayEquals([], chrome.readingMode.getLanguagesEnabledInPref());
  });

  test('restoreFromPrefs adds initially populated languages to prefs', () => {
    const previouslyAvailableLang = 'pt-pt';
    const availableLang = 'pt-br';
    chrome.readingMode.onLanguagePrefChange(previouslyAvailableLang, true);
    voiceLanguageController.enableLang(availableLang);
    createAndSetVoices(speech, [
      {lang: availableLang, name: 'Google Galinda'},
    ]);

    voiceLanguageController.restoreFromPrefs();

    assertArrayEquals(
        [availableLang], chrome.readingMode.getLanguagesEnabledInPref());
  });

  // <if expr="not is_chromeos">
  test(
      'restoreFromPrefs adds unavailable language to prefs once available',
      () => {
        const previouslyAvailableLang = 'da-dk';
        chrome.readingMode.onLanguagePrefChange(previouslyAvailableLang, true);
        createAndSetVoices(speech, [
          {lang: 'en-us', name: 'Google Fiyero'},
        ]);
        voiceLanguageController.restoreFromPrefs();

        assertArrayEquals([], chrome.readingMode.getLanguagesEnabledInPref());

        // The previously unavailable language is now available.
        voiceLanguageController.enableLang(previouslyAvailableLang);
        createAndSetVoices(speech, [
          {lang: 'en-us', name: 'Google Fiyero'},
          {lang: 'da-dk', name: 'Doctor Dillamond'},
        ]);
        voiceLanguageController.restoreFromPrefs();

        assertArrayEquals(
            [previouslyAvailableLang],
            chrome.readingMode.getLanguagesEnabledInPref());
      });
  // </if>

  suite('restoreFromPrefs populates enabled languages', () => {
    const langs = ['si', 'km', 'th'];
    const locales = ['si-lk', 'km-kh', 'th-th'];

    setup(() => {
      createAndSetVoices(speech, [
        {lang: langs[0], name: 'Google Frodo'},
        {lang: langs[1], name: 'Google Merry'},
        {lang: langs[2], name: 'Google Pippin'},
      ]);
    });

    test('with langs stored in prefs', () => {
      chrome.readingMode.getLanguagesEnabledInPref = () => langs;

      voiceLanguageController.restoreFromPrefs();

      assertTrue(onEnabledLangsChange);
      assertArrayEquals(
          langs.concat(locales), voiceLanguageController.getEnabledLangs());
    });

    test('with browser lang', () => {
      chrome.readingMode.baseLanguageForSpeech = langs[1]!;

      voiceLanguageController.restoreFromPrefs();

      assertTrue(onEnabledLangsChange);
      assertArrayEquals(
          [langs[1], locales[1]], voiceLanguageController.getEnabledLangs());
    });
  });

  test('restoreFromPrefs enables the lang for the preferred voice', () => {
    speech.setVoices(voices);
    chrome.readingMode.getStoredVoice = () => otherVoice.name;

    voiceLanguageController.restoreFromPrefs();

    assertTrue(voiceLanguageController.isLangEnabled(otherVoice.lang));
  });

  test('restoreFromPrefs uses the stored voice for this language', () => {
    speech.setVoices(voices);
    chrome.readingMode.getStoredVoice = () => otherVoice.name;

    voiceLanguageController.restoreFromPrefs();

    assertTrue(onCurrentVoiceChange);
    assertEquals(otherVoice, voiceLanguageController.getCurrentVoice());
  });

  test(
      'restoreFromPrefs uses the default voice if the stored voice is invalid',
      () => {
        speech.setVoices(voices);
        chrome.readingMode.getStoredVoice = () => 'Matt';
        voiceLanguageController.enableLang(langForDefaultVoice);

        voiceLanguageController.restoreFromPrefs();

        assertTrue(onCurrentVoiceChange);
        assertEquals(defaultVoice, voiceLanguageController.getCurrentVoice());
      });

  test('restoreFromPrefs installs exactly matching enabled langs', () => {
    const lang1 = 'km';
    const lang1Exact = 'km-kh';
    const lang2 = 'de';
    const lang3 = 'cs';
    voiceLanguageController.enableLang(lang1Exact);
    voiceLanguageController.enableLang(lang2);
    voiceLanguageController.enableLang(lang3);

    voiceLanguageController.restoreFromPrefs();
    assertArrayEquals([lang1], requestInfoLangs);

    voiceLanguageController.updateLanguageStatus(lang1, 'kNotInstalled');
    voiceLanguageController.updateLanguageStatus(lang2, 'kNotInstalled');
    voiceLanguageController.updateLanguageStatus(lang3, 'kNotInstalled');
    assertEquals(
        VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
        voiceLanguageController.getLocalStatus(lang1));
    assertFalse(!!voiceLanguageController.getLocalStatus(lang2));
    assertFalse(!!voiceLanguageController.getLocalStatus(lang3));
    assertArrayEquals([lang1], installedLangs);
  });

  test('onLanguageToggle enabled languages are added', () => {
    const firstLanguage = 'en-us';
    voiceLanguageController.onLanguageToggle(firstLanguage);
    assertTrue(voiceLanguageController.isLangEnabled(firstLanguage));
    assertTrue(
        chrome.readingMode.getLanguagesEnabledInPref().includes(firstLanguage));

    const secondLanguage = 'fr';
    voiceLanguageController.onLanguageToggle(secondLanguage);
    assertTrue(voiceLanguageController.isLangEnabled(secondLanguage));
    assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
        secondLanguage));
  });

  test('onLanguageToggle disabled languages are removed', () => {
    const firstLanguage = 'en-us';
    voiceLanguageController.onLanguageToggle(firstLanguage);
    assertTrue(voiceLanguageController.isLangEnabled(firstLanguage));
    assertTrue(
        chrome.readingMode.getLanguagesEnabledInPref().includes(firstLanguage));

    voiceLanguageController.onLanguageToggle(firstLanguage);
    assertFalse(voiceLanguageController.isLangEnabled(firstLanguage));
    assertFalse(
        chrome.readingMode.getLanguagesEnabledInPref().includes(firstLanguage));
  });

  test('onLanguageToggle with voice pack lang uninstalls it', () => {
    const lang = 'km';
    voiceLanguageController.onLanguageToggle(lang);
    VoiceNotificationManager.getInstance().onVoiceStatusChange(
        lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);

    voiceLanguageController.onLanguageToggle(lang);
    assertEquals(NotificationType.NONE, notificationType);
    assertArrayEquals([lang], uninstalledLangs);
  });

  test('onLanguageToggle with non voice pack lang does not uninstall', () => {
    const lang = 'zh';
    voiceLanguageController.onLanguageToggle(lang);
    notificationType = null;

    voiceLanguageController.onLanguageToggle(lang);

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
        voiceLanguageController.updateLanguageStatus(lang, 'kOther');

        voiceLanguageController.onLanguageToggle(lang);

        assertArrayEquals([lang], installedLangs);
        assertEquals(
            voiceLanguageController.getLocalStatus(lang),
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY);
      });

  test(
      'onLanguageToggle when there is no status for lang, installs lang',
      () => {
        voiceLanguageController.onLanguageToggle('en-us');
        assertArrayEquals(['en-us'], installedLangs);
      });


  test(
      'onLanguageToggle when language status is uninstalled, does not install',
      () => {
        const lang = 'en-us';
        voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');

        voiceLanguageController.onLanguageToggle(lang);

        assertArrayEquals([], installedLangs);
      });

  test('onVoicesChanged with auto selected voice, uses a Natural voice', () => {
    chrome.readingMode.getStoredVoice = () => '';
    const voice =
        createSpeechSynthesisVoice({lang: 'ja', name: 'Google Eagle'});
    const naturalVoice = createSpeechSynthesisVoice(
        {lang: 'ja', name: 'Google Horse (Natural)'});
    speech.setVoices([voice, naturalVoice]);
    voiceLanguageController.setUserPreferredVoice(voice);
    chrome.readingMode.baseLanguageForSpeech = voice.lang;
    voiceLanguageController.onPageLanguageChanged();

    voiceLanguageController.onVoicesChanged();

    assertTrue(onAvailableVoicesChange);
    assertEquals(naturalVoice, voiceLanguageController.getCurrentVoice());
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
        voiceLanguageController.setUserPreferredVoice(voice);
        chrome.readingMode.baseLanguageForSpeech = voice.lang;
        voiceLanguageController.onPageLanguageChanged();

        voiceLanguageController.onVoicesChanged();

        assertTrue(onAvailableVoicesChange);
        assertEquals(voice, voiceLanguageController.getCurrentVoice());
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
    voiceLanguageController.restoreFromPrefs();
    assertArrayEquals([lang1], voiceLanguageController.getEnabledLangs());
    assertArrayEquals([lang1], chrome.readingMode.getLanguagesEnabledInPref());
    onEnabledLangsChange = false;

    speech.setVoices([
      createSpeechSynthesisVoice({lang: lang1, name: 'Henry'}),
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Thomas'}),
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Matt'}),
    ]);
    voiceLanguageController.onVoicesChanged();

    // After voices come in, we should enable those langs.
    voiceLanguageController.restoreFromPrefs();
    assertArrayEquals(
        [lang1, lang2, lang3], voiceLanguageController.getEnabledLangs());
    assertArrayEquals(
        [lang1, lang2, lang3], chrome.readingMode.getLanguagesEnabledInPref());
    assertTrue(voiceLanguageController.isLangEnabled(lang1));
    assertTrue(voiceLanguageController.isLangEnabled(lang2));
    assertTrue(voiceLanguageController.isLangEnabled(lang3));
    assertTrue(onEnabledLangsChange);
  });
  // </if>

  test('onVoicesChanged after new tts engine installs google locales', () => {
    const lang1 = 'bn-bd';
    const lang2 = 'hu-hu';
    const lang3 = 'en';
    voiceLanguageController.enableLang(lang1);
    voiceLanguageController.enableLang(lang2);
    voiceLanguageController.enableLang(lang3);
    voiceLanguageController.onTtsEngineInstalled();
    installedLangs = [];

    voiceLanguageController.onVoicesChanged();

    assertArrayEquals(['bn', 'hu'], installedLangs);
    assertTrue(onAvailableVoicesChange);
    assertFalse(voiceLanguageController.hasAvailableVoices());
  });

  test(
      'onVoicesChanged after new tts engine enables page language if no ' +
          'voices before install',
      () => {
        chrome.readingMode.getStoredVoice = () => '';
        voiceLanguageController.onTtsEngineInstalled();
        const lang = 'de';
        chrome.readingMode.baseLanguageForSpeech = lang;
        voiceLanguageController.onVoicesChanged();

        // onVoicesChanged should request an install for the page language.
        assertArrayEquals([lang], requestInfoLangs);
        voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
        assertEquals(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
            voiceLanguageController.getLocalStatus(lang));
        assertArrayEquals([lang], installedLangs);
      });

  test('onTtsEngineInstalled installs enabled google locales', () => {
    const lang1 = 'bn-bd';
    const lang2 = 'hu-hu';
    const lang3 = 'en';
    voiceLanguageController.enableLang(lang1);
    voiceLanguageController.enableLang(lang2);
    voiceLanguageController.enableLang(lang3);

    voiceLanguageController.onTtsEngineInstalled();

    assertArrayEquals(['bn', 'hu'], installedLangs);
    assertTrue(onAvailableVoicesChange);
    assertFalse(voiceLanguageController.hasAvailableVoices());
  });

  test(
      'onTtsEngineInstalled enables page language if no voices before install',
      () => {
        chrome.readingMode.getStoredVoice = () => '';
        const voice =
            createSpeechSynthesisVoice({lang: 'de-de', name: 'Google German'});
        // Change page language to de before any voices are available for it.
        const lang = 'de';
        chrome.readingMode.baseLanguageForSpeech = lang;
        voiceLanguageController.onPageLanguageChanged();

        // onTtsEngineInstalled should request an install for the page language,
        // but it's not enabled yet.
        voiceLanguageController.onTtsEngineInstalled();
        assertFalse(voiceLanguageController.getEnabledLangs().includes(lang));
        assertTrue(requestInfoLangs.includes(lang));

        // Once the status comes back as not installed, then actually request
        // the install.
        voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
        assertEquals(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
            voiceLanguageController.getLocalStatus(lang));
        assertArrayEquals([lang], installedLangs);

        // When the install completes, new voices for the requested language
        // should be available, and the installed status should come back. At
        // that point, the page language should be enabled.
        speech.setVoices([voice]);
        voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');
        assertTrue(voiceLanguageController.getEnabledLangs().includes('de-de'));
      });

  test('onVoicesChanged restores from prefs on first voices received', () => {
    const lang = 'uk';
    const name = 'Google Lemur';
    const voice = createSpeechSynthesisVoice({lang, name});
    speech.setVoices([voice]);
    chrome.readingMode.getStoredVoice = () => name;

    voiceLanguageController.onVoicesChanged();

    assertTrue(voiceLanguageController.isLangEnabled(lang));
    assertTrue(onAvailableVoicesChange);
    assertArrayEquals([voice], voiceLanguageController.getAvailableVoices());
  });

  test('onVoicesChanged requests info', () => {
    const lang1 = 'fi';
    const lang2 = 'id';
    const lang3 = 'da';
    voiceLanguageController.setServerStatus(
        lang1, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
    voiceLanguageController.setServerStatus(
        lang2, mojoVoicePackStatusToVoicePackStatusEnum('kAllocation'));
    voiceLanguageController.setServerStatus(
        lang3, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));

    voiceLanguageController.onVoicesChanged();

    assertTrue(onAvailableVoicesChange);
    assertArrayEquals([lang1, lang2, lang3], requestInfoLangs);
  });

  test('onVoicesChanged waits for engine timeout', () => {
    const lang = 'fi';
    voiceLanguageController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
    const mockTimer = new MockTimer();
    mockTimer.install();

    voiceLanguageController.onVoicesChanged();
    mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
    mockTimer.uninstall();

    assertEquals(NotificationType.GOOGLE_VOICES_UNAVAILABLE, notificationType);
  });

  test(
      'onVoicesChanged does nothing when current voice' +
          ' still available',
      () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'id', name: 'Google Dog'});
        speech.setVoices([voice]);
        voiceLanguageController.enableLang(voice.lang);
        voiceLanguageController.setUserPreferredVoice(voice);
        onCurrentVoiceChange = false;

        voiceLanguageController.onVoicesChanged();

        assertFalse(onCurrentVoiceChange);
        assertTrue(onAvailableVoicesChange);
        assertEquals(voice, voiceLanguageController.getCurrentVoice());
      });

  test(
      'onVoicesChanged gets default voice when current voice unavailable',
      () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'id', name: 'Google Cat'});
        const defaultVoice =
            createSpeechSynthesisVoice({lang: 'id', name: 'Google Komodo'});
        speech.setVoices([defaultVoice]);
        voiceLanguageController.enableLang(voice.lang);
        voiceLanguageController.setUserPreferredVoice(voice);
        onCurrentVoiceChange = false;

        voiceLanguageController.onVoicesChanged();

        assertTrue(onCurrentVoiceChange);
        assertTrue(onAvailableVoicesChange);
        assertEquals(defaultVoice, voiceLanguageController.getCurrentVoice());
      });

  test(
      'onVoicesChanged gets default voice when current voice unavailable and' +
          ' no voices for current language',
      () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'zh-CN', name: 'Google Tiger'});
        const defaultVoice =
            createSpeechSynthesisVoice({lang: 'en-us', name: 'Google Bear'});
        speech.setVoices([defaultVoice]);
        voiceLanguageController.enableLang(voice.lang);
        voiceLanguageController.setUserPreferredVoice(voice);
        chrome.readingMode.baseLanguageForSpeech = 'zh-CN';
        onCurrentVoiceChange = false;

        voiceLanguageController.onVoicesChanged();

        assertTrue(onCurrentVoiceChange);
        assertTrue(onAvailableVoicesChange);
        assertEquals(defaultVoice, voiceLanguageController.getCurrentVoice());
      });

  test('onVoicesChanged gets stored voice', () => {
    const voice1 =
        createSpeechSynthesisVoice({lang: 'id', name: 'Google Tiger'});
    const voice2 =
        createSpeechSynthesisVoice({lang: 'id', name: 'Google Moose'});
    speech.setVoices([voice1, voice2]);
    chrome.readingMode.getStoredVoice = () => voice1.name;
    voiceLanguageController.enableLang(voice1.lang);
    onCurrentVoiceChange = false;

    voiceLanguageController.onVoicesChanged();

    assertTrue(onCurrentVoiceChange);
    assertTrue(onAvailableVoicesChange);
    assertEquals(voice1, voiceLanguageController.getCurrentVoice());
  });

  test('stopWaitingForSpeechExtension stops waiting for engine timeout', () => {
    const lang = 'fi';
    voiceLanguageController.setServerStatus(
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

    voiceLanguageController.onVoicesChanged();
    voiceLanguageController.stopWaitingForSpeechExtension();
    mockTimer.tick(EXTENSION_RESPONSE_TIMEOUT_MS);
    mockTimer.uninstall();

    // Now download the voice since the speech engine responded.
    assertEquals(NotificationType.DOWNLOADING, notificationType);
  });

  test('onPageLanguageChanged updates current language', () => {
    const lang = 'el';
    chrome.readingMode.baseLanguageForSpeech = lang;

    voiceLanguageController.onPageLanguageChanged();

    assertEquals(lang, voiceLanguageController.getCurrentLanguage());
  });

  test(
      'onPageLanguageChanged updates current language when natural voices unavailable',
      () => {
        chrome.readingMode.getStoredVoice = () => '';
        const voice1 =
            createSpeechSynthesisVoice({lang: 'en-us', name: 'Google English'});
        const voice2 =
            createSpeechSynthesisVoice({lang: 'de-de', name: 'Google German'});
        speech.setVoices([voice1, voice2]);
        voiceLanguageController.onVoicesChanged();
        const lang = 'de';
        chrome.readingMode.baseLanguageForSpeech = lang;

        voiceLanguageController.onPageLanguageChanged();

        assertEquals(voice2, voiceLanguageController.getCurrentVoice());
      });

  test('onPageLanguageChanged installs lang if no status', () => {
    const lang = 'en-gb';
    voiceLanguageController.enableLang(lang);

    chrome.readingMode.baseLanguageForSpeech = lang;
    voiceLanguageController.onPageLanguageChanged();
    assertArrayEquals([lang], requestInfoLangs);

    voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
    assertEquals(
        VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
        voiceLanguageController.getLocalStatus(lang));
    assertArrayEquals([lang], installedLangs);
  });

  test('onPageLanguageChanged installs lang if not installed', () => {
    const lang = 'es-ES';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voiceLanguageController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled'));


    voiceLanguageController.onPageLanguageChanged();
    assertArrayEquals([lang.toLowerCase()], requestInfoLangs);

    voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');
    assertEquals(
        VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
        voiceLanguageController.getLocalStatus(lang));
    assertArrayEquals([lang.toLowerCase()], installedLangs);
  });

  test('onPageLanguageChanged when previously failed does not install', () => {
    const lang = 'es-ES';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voiceLanguageController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));

    voiceLanguageController.onPageLanguageChanged();

    assertFalse(!!voiceLanguageController.getLocalStatus(lang));
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], installedLangs);
  });

  test('onPageLanguageChanged and already installing does not install', () => {
    const lang = 'es-ES';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voiceLanguageController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalling'));

    voiceLanguageController.onPageLanguageChanged();

    assertFalse(!!voiceLanguageController.getLocalStatus(lang));
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], installedLangs);
  });

  test('onPageLanguageChanged and already installed does not install', () => {
    const lang = 'es-ES';
    chrome.readingMode.baseLanguageForSpeech = lang;
    voiceLanguageController.setServerStatus(
        lang, mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));

    voiceLanguageController.onPageLanguageChanged();

    assertFalse(!!voiceLanguageController.getLocalStatus(lang));
    assertArrayEquals([], requestInfoLangs);
    assertArrayEquals([], installedLangs);
  });

  test('onPageLanguageChanged doesn\'t install unsupported language', () => {
    chrome.readingMode.baseLanguageForSpeech = 'zh';

    voiceLanguageController.onPageLanguageChanged();

    // Use this check to ensure this stays updated if the supported
    // languages changes.
    assertFalse(PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(
        chrome.readingMode.baseLanguageForSpeech));
    assertArrayEquals([], requestInfoLangs);
  });

  test('onPageLanguageChanged installs without exact match', () => {
    const lang = 'bn';
    chrome.readingMode.baseLanguageForSpeech = lang;

    voiceLanguageController.onPageLanguageChanged();

    // Use these checks to ensure this stays updated if the supported
    // languages changes.
    assertTrue(PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(lang));
    assertFalse(AVAILABLE_GOOGLE_TTS_LOCALES.has(lang));
    assertArrayEquals([lang], requestInfoLangs);
  });

  test('onPageLanguageChanged uses the stored voice for this language', () => {
    speech.setVoices(voices);
    chrome.readingMode.getStoredVoice = () => otherVoice.name;

    voiceLanguageController.onPageLanguageChanged();

    assertTrue(onCurrentVoiceChange);
    assertEquals(otherVoice, voiceLanguageController.getCurrentVoice());
  });

  test('onPageLanguageChanged with no Google voices, uses system voice', () => {
    const lang = 'zh-CN';
    const voice = createSpeechSynthesisVoice({lang, name: 'Conan'});
    speech.setVoices([voice]);
    voiceLanguageController.onVoicesChanged();
    chrome.readingMode.baseLanguageForSpeech = lang;

    voiceLanguageController.onPageLanguageChanged();

    assertTrue(onCurrentVoiceChange);
    assertEquals(voice, voiceLanguageController.getCurrentVoice());
  });

  test(
      'onPageLanguageChanged uses default voice if the stored voice is invalid',
      () => {
        speech.setVoices(voices);
        chrome.readingMode.getStoredVoice = () => 'Matt';
        voiceLanguageController.enableLang(langForDefaultVoice);

        voiceLanguageController.onPageLanguageChanged();

        assertTrue(onCurrentVoiceChange);
        assertEquals(defaultVoice, voiceLanguageController.getCurrentVoice());
      });

  suite('onPageLanguageChanged with no stored voice for this language', () => {
    setup(() => {
      chrome.readingMode.getStoredVoice = () => '';
      speech.setVoices(voices);
      voiceLanguageController.setServerStatus(
          lang1, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));
      voiceLanguageController.setServerStatus(
          lang2, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));
      voiceLanguageController.setServerStatus(
          lang3, mojoVoicePackStatusToVoicePackStatusEnum('kOther'));
    });

    suite('and no voices at all for this language', () => {
      setup(() => {
        chrome.readingMode.baseLanguageForSpeech = langWithNoVoices;
      });

      test('uses the current voice if there is one', () => {
        voiceLanguageController.setUserPreferredVoice(otherVoice);
        voiceLanguageController.onPageLanguageChanged();
        assertEquals(otherVoice, voiceLanguageController.getCurrentVoice());
      });

      test('uses a natural voice if there\'s no current voice', () => {
        voiceLanguageController.onPageLanguageChanged();
        assertEquals(
            naturalVoiceWithLang3, voiceLanguageController.getCurrentVoice());
      });

      test('uses the device default if there\'s no natural', () => {
        speech.setVoices(voices.filter(v => v !== naturalVoiceWithLang3));
        voiceLanguageController.onVoicesChanged();

        voiceLanguageController.onPageLanguageChanged();

        assertEquals(
            defaultVoice, voiceLanguageController.getCurrentVoice(),
            voiceLanguageController.getCurrentVoice()?.name);
      });
    });

    test('enables pack manager locale', () => {
      chrome.readingMode.baseLanguageForSpeech = lang3;
      voiceLanguageController.onVoicesChanged();

      voiceLanguageController.onPageLanguageChanged();

      assertTrue(voiceLanguageController.isLangEnabled(lang3));
      assertEquals(
          naturalVoiceWithLang3, voiceLanguageController.getCurrentVoice());
    });

    test('enables other locale if not supported by pack manager', () => {
      chrome.readingMode.baseLanguageForSpeech = lang1;
      voiceLanguageController.onVoicesChanged();

      voiceLanguageController.onPageLanguageChanged();

      assertTrue(voiceLanguageController.isLangEnabled(lang1));
      assertEquals(
          defaultVoiceWithLang1, voiceLanguageController.getCurrentVoice());
    });

    test('uses a natural voice for this language', () => {
      chrome.readingMode.baseLanguageForSpeech = lang3;
      voiceLanguageController.enableLang(lang3);

      voiceLanguageController.onPageLanguageChanged();

      assertEquals(
          naturalVoiceWithLang3, voiceLanguageController.getCurrentVoice());
    });

    test(
        'uses the default voice for this language with no natural voice',
        () => {
          chrome.readingMode.baseLanguageForSpeech = lang1;
          voiceLanguageController.enableLang(lang1);

          voiceLanguageController.onPageLanguageChanged();

          assertEquals(
              defaultVoiceWithLang1, voiceLanguageController.getCurrentVoice());
        });

    test(
        'uses the first listed voice for this language if there\'s no default',
        () => {
          chrome.readingMode.baseLanguageForSpeech = lang2;
          voiceLanguageController.enableLang(lang2);

          voiceLanguageController.onPageLanguageChanged();

          assertEquals(
              firstVoiceWithLang2, voiceLanguageController.getCurrentVoice());
        });


    test('uses a voice in a different locale but same language', () => {
      chrome.readingMode.baseLanguageForSpeech = 'en-US';
      voiceLanguageController.enableLang('en-gb');
      const voice = createSpeechSynthesisVoice(
          {lang: 'en-GB', name: 'British', default: true});
      setVoices(speech, [voice]);
      voiceLanguageController.setServerStatus(
          'en-gb', mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));
      voiceLanguageController.setServerStatus(
          'en-us', mojoVoicePackStatusToVoicePackStatusEnum('kInstalled'));

      voiceLanguageController.onPageLanguageChanged();

      assertEquals(voice, voiceLanguageController.getCurrentVoice());
    });

    test('uses a natural enabled voice if no same locale', () => {
      voiceLanguageController.enableLang(lang3);
      chrome.readingMode.baseLanguageForSpeech = lang2;

      voiceLanguageController.onPageLanguageChanged();

      assertEquals(
          naturalVoiceWithLang3, voiceLanguageController.getCurrentVoice());
    });

    test('uses a default enabled voice if no natural voice', () => {
      voiceLanguageController.enableLang(lang1);
      chrome.readingMode.baseLanguageForSpeech = lang2;

      voiceLanguageController.onPageLanguageChanged();

      assertEquals(
          defaultVoiceWithLang1, voiceLanguageController.getCurrentVoice());
    });

    test('no voice if no enabled languages', () => {
      chrome.readingMode.baseLanguageForSpeech = lang2;
      for (const lang of voiceLanguageController.getEnabledLangs()) {
        voiceLanguageController.onLanguageToggle(lang);
      }

      voiceLanguageController.onPageLanguageChanged();

      assertFalse(!!voiceLanguageController.getCurrentVoice());
    });
  });

  suite('updateLanguageStatus', () => {
    const lang = 'pt-br';

    setup(() => {
      voiceLanguageController.enableLang(lang);
      chrome.readingMode.onLanguagePrefChange(lang, true);
    });

    test('with no lang does nothing', () => {
      voiceLanguageController.updateLanguageStatus('', 'kInstalled');
      assertEquals(null, notificationType);
      assertArrayEquals([], installedLangs);
    });

    test('with no lang and not reached status notifies of no engine', () => {
      voiceLanguageController.updateLanguageStatus('', 'kNotReached');
      assertEquals(
          NotificationType.GOOGLE_VOICES_UNAVAILABLE, notificationType);
      assertArrayEquals([], installedLangs);
    });

    test('with lang not marked for download does not install', () => {
      voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');

      assertEquals(
          VoiceClientSideStatusCode.NOT_INSTALLED,
          voiceLanguageController.getLocalStatus(lang));
      assertArrayEquals([], installedLangs);
    });

    test('with lang marked for download requests install', () => {
      chrome.readingMode.baseLanguageForSpeech = lang;
      voiceLanguageController.onPageLanguageChanged();

      voiceLanguageController.updateLanguageStatus(lang, 'kNotInstalled');

      assertEquals(
          VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
          voiceLanguageController.getLocalStatus(lang));
      const serverStatus = voiceLanguageController.getServerStatus(lang);
      assertTrue(!!serverStatus);
      assertEquals(
          VoicePackServerStatusSuccessCode.NOT_INSTALLED, serverStatus.code);
      assertEquals('Successful response', serverStatus.id);
      assertArrayEquals([lang], installedLangs);
    });

    test('with no other voices for language, disables language', () => {
      setVoices(speech, []);

      voiceLanguageController.updateLanguageStatus(lang, 'kOther');

      assertFalse(voiceLanguageController.isLangEnabled(lang));
      assertFalse(
          chrome.readingMode.getLanguagesEnabledInPref().includes(lang));
    });

    // <if expr="is_chromeos">
    test('chromeOS should disable if no google voices', () => {
      const lang1 = 'en-US';
      const lang2 = 'fr';
      const lang3 = 'yue';
      voiceLanguageController.enableLang(lang1);
      chrome.readingMode.onLanguagePrefChange(lang1.toLowerCase(), true);
      voiceLanguageController.enableLang(lang2);
      chrome.readingMode.onLanguagePrefChange(lang2, true);
      voiceLanguageController.enableLang(lang3);
      chrome.readingMode.onLanguagePrefChange(lang3, true);
      createAndSetVoices(speech, [
        {lang: lang1, name: 'Henry'},
        {lang: lang2, name: 'Google Thomas'},
      ]);

      voiceLanguageController.updateLanguageStatus(lang1, 'kOther');
      voiceLanguageController.updateLanguageStatus(lang2, 'kOther');
      voiceLanguageController.updateLanguageStatus(lang3, 'kOther');

      const langsInPrefs = chrome.readingMode.getLanguagesEnabledInPref();
      assertFalse(langsInPrefs.includes(lang1.toLowerCase()));
      assertTrue(langsInPrefs.includes(lang2));
      assertFalse(langsInPrefs.includes(lang3));
      assertFalse(voiceLanguageController.isLangEnabled(lang1));
      assertTrue(voiceLanguageController.isLangEnabled(lang2));
      assertFalse(voiceLanguageController.isLangEnabled(lang3));
    });
    // </if>

    // <if expr="not is_chromeos">
    test('desktop should only disable if no voices at all', () => {
      const lang1 = 'en-US';
      const lang2 = 'fr';
      const lang3 = 'yue';
      voiceLanguageController.enableLang(lang1);
      chrome.readingMode.onLanguagePrefChange(lang1.toLowerCase(), true);
      voiceLanguageController.enableLang(lang2);
      chrome.readingMode.onLanguagePrefChange(lang2, true);
      voiceLanguageController.enableLang(lang3);
      chrome.readingMode.onLanguagePrefChange(lang3, true);
      createAndSetVoices(speech, [
        {lang: lang1, name: 'Henry'},
        {lang: lang2, name: 'Google Thomas'},
      ]);
      onEnabledLangsChange = false;

      voiceLanguageController.updateLanguageStatus(lang1, 'kOther');
      voiceLanguageController.updateLanguageStatus(lang2, 'kOther');
      voiceLanguageController.updateLanguageStatus(lang3, 'kOther');

      const langsInPrefs = chrome.readingMode.getLanguagesEnabledInPref();
      assertTrue(langsInPrefs.includes(lang1.toLowerCase()));
      assertTrue(langsInPrefs.includes(lang2));
      assertFalse(langsInPrefs.includes(lang3), 'lang3 prefs');
      assertTrue(voiceLanguageController.isLangEnabled(lang1));
      assertTrue(voiceLanguageController.isLangEnabled(lang2));
      assertFalse(voiceLanguageController.isLangEnabled(lang3), 'lang3');
    });
    // </if>

    test(
        'when language-pack lang does not match voice lang, still disables it',
        () => {
          voiceLanguageController.enableLang('it-it');
          setVoices(speech, []);

          voiceLanguageController.updateLanguageStatus('it', 'kOther');

          assertFalse(
              voiceLanguageController.isLangEnabled('it-it'), 'controller');
          assertFalse(
              chrome.readingMode.getLanguagesEnabledInPref().includes('it-it'),
              'prefs');
        });

    test(
        'when language-pack lang does not match voice lang, with ' +
            'e-speak voices, still disables language',
        () => {
          voiceLanguageController.enableLang('it-it');
          createAndSetVoices(speech, [
            {lang: 'it', name: 'eSpeak Italian '},
          ]);

          voiceLanguageController.updateLanguageStatus('it', 'kOther');

          assertFalse(voiceLanguageController.isLangEnabled('it-it'));
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
          voiceLanguageController.updateLanguageStatus(lang, 'kOther');

          assertTrue(voiceLanguageController.isLangEnabled(lang), 'controller');
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

          voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

          const serverStatus = voiceLanguageController.getServerStatus(lang);
          assertTrue(!!serverStatus, 'status is: ' + serverStatus?.id);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voiceLanguageController.getLocalStatus(lang));
        });

    test(
        'unavailable if system voices are in the list for a different lang',
        () => {
          const lang = 'de';

          // Installed 'de' language pack, but the fake available voice list
          // only has english voices.
          voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

          const serverStatus = voiceLanguageController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voiceLanguageController.getLocalStatus(lang));
        });

    test(
        'unavailable if only system voices are in the list for this lang',
        () => {
          const lang = 'en';

          voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

          const serverStatus = voiceLanguageController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voiceLanguageController.getLocalStatus(lang));
        });

    test(
        'available if natural voices are unsupported for this lang and voices' +
            ' are available',
        () => {
          const lang = 'yue';
          createAndSetVoices(speech, [
            {lang: 'yue-hk', name: 'Cantonese'},
          ]);

          voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

          const serverStatus = voiceLanguageController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.AVAILABLE,
              voiceLanguageController.getLocalStatus(lang));
        });

    test(
        'unavailable if natural voices are unsupported for this lang and ' +
            'voices unavailable',
        () => {
          const lang = 'yue';

          voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

          const serverStatus = voiceLanguageController.getServerStatus(lang);
          assertTrue(!!serverStatus);
          assertEquals(
              VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
          assertEquals('Successful response', serverStatus.id);
          assertEquals(
              VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
              voiceLanguageController.getLocalStatus(lang));
        });

    test('available if natural voices are installed for this lang', () => {
      const lang = 'en-us';
      // set installing status so that the old status is not empty.
      voiceLanguageController.updateLanguageStatus(lang, 'kInstalling');
      // set the voices on speech synthesis without triggering on voices
      // changed, so we can verify that updateLanguageStatus calls it.
      createAndSetVoices(speech, [
        {lang: lang, name: 'Wall-e (Natural)'},
        {lang: lang, name: 'Andy (Natural)'},
      ]);
      voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

      const serverStatus = voiceLanguageController.getServerStatus(lang);
      assertTrue(!!serverStatus);
      assertEquals(
          VoicePackServerStatusSuccessCode.INSTALLED, serverStatus.code);
      assertEquals('Successful response', serverStatus.id);
      // This would be INSTALLED_AND_UNAVIALABLE if the voice list wasn't
      // refreshed.
      assertEquals(
          VoiceClientSideStatusCode.AVAILABLE,
          voiceLanguageController.getLocalStatus(lang));
    });

    test(
        'uses newly available voices if it\'s for the current language', () => {
          const lang = 'en-us';
          chrome.readingMode.baseLanguageForSpeech = lang;
          voiceLanguageController.enableLang(lang);
          chrome.readingMode.getStoredVoice = () => '';
          createAndSetVoices(
              speech, [{lang: lang, name: 'Google Cow (Natural)'}]);
          voiceLanguageController.updateLanguageStatus(lang, 'kInstalled');

          const selectedVoice = voiceLanguageController.getCurrentVoice();
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
          voiceLanguageController.enableLang(
              chrome.readingMode.baseLanguageForSpeech);
          const currentVoice = createSpeechSynthesisVoice({
            name: 'Portuguese voice 1',
            lang: chrome.readingMode.baseLanguageForSpeech,
          });
          voiceLanguageController.setUserPreferredVoice(currentVoice);
          chrome.readingMode.getStoredVoice = () => '';
          setVoices(speech, [currentVoice]);

          voiceLanguageController.updateLanguageStatus(
              installedLang, 'kInstalled');

          // The selected voice should stay the same as it was.
          assertEquals(currentVoice, voiceLanguageController.getCurrentVoice());
        });

    test('with error code marks the status', () => {
      const lang = 'en-us';

      voiceLanguageController.updateLanguageStatus(lang, 'kOther');

      const serverStatus = voiceLanguageController.getServerStatus(lang);
      assertTrue(!!serverStatus);
      assertEquals(serverStatus.code, VoicePackServerStatusErrorCode.OTHER);
      assertEquals('Unsuccessful response', serverStatus.id);
      assertEquals(
          voiceLanguageController.getLocalStatus(lang),
          VoiceClientSideStatusCode.ERROR_INSTALLING);
    });
  });

  test('onLanguageUnavailableError chooses new language', () => {
    const pageLanguage = 'es';
    const otherLanguage = 'tr';
    chrome.readingMode.baseLanguageForSpeech = pageLanguage;
    voiceLanguageController.onPageLanguageChanged();
    chrome.readingMode.defaultLanguageForSpeech = otherLanguage;
    speech.setVoices([createSpeechSynthesisVoice(
        {lang: otherLanguage, name: 'Google Scorpion'})]);
    voiceLanguageController.onVoicesChanged();

    voiceLanguageController.onLanguageUnavailableError();

    assertEquals(otherLanguage, voiceLanguageController.getCurrentLanguage());
  });

  test('voiceUnavailable selects default voice', () => {
    const voice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Google Giraffe'});
    speech.setVoices([voice]);

    voiceLanguageController.onVoiceUnavailableError();

    assertEquals(voice, voiceLanguageController.getCurrentVoice());
  });

  test(
      'voiceUnavailable default voice is current voice, selects another voice',
      () => {
        const voice1 =
            createSpeechSynthesisVoice({lang: 'en', name: 'Google George'});
        const voice2 =
            createSpeechSynthesisVoice({lang: 'en', name: 'Google Connie'});
        voiceLanguageController.setUserPreferredVoice(voice1);
        speech.setVoices([voice1, voice2]);

        voiceLanguageController.onVoiceUnavailableError();

        assertEquals(voice2, voiceLanguageController.getCurrentVoice());
      });

  test(
      'voiceUnavailable continues to select default voice if no voices ' +
          'available in language',
      () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'en', name: 'Google Penguin'});
        speech.setVoices([voice]);

        voiceLanguageController.onVoiceUnavailableError();

        assertEquals(voice, voiceLanguageController.getCurrentVoice());
      });
});
