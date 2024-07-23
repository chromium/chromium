// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, VoiceClientSideStatusCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';

suite('LanguageMenu', () => {
  let languageMenu: LanguageMenuElement;
  let availableVoices: SpeechSynthesisVoice[];
  let enabledLangs: string[];
  const languagesToNotificationMap:
      {[language: string]: VoiceClientSideStatusCode} = {};


  function setAvailableVoices() {
    languageMenu.availableVoices = availableVoices;
    flush();
  }

  function setEnabledLanguages() {
    languageMenu.enabledLangs = enabledLangs;
    flush();
  }

  function setNotificationForLanguage() {
    languageMenu.voicePackInstallStatus = {...languagesToNotificationMap};
    flush();
  }

  function getLanguageLineItems() {
    return languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
        '.language-line');
  }

  function getNotificationItems() {
    return languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
        '#notificationText');
  }

  function getLanguageSearchField() {
    return languageMenu.$.languageMenu.querySelector<CrInputElement>(
        '.search-field')!;
  }

  function getNoResultsFoundMessage() {
    return languageMenu.$.languageMenu.querySelector<HTMLElement>(
        '#noResultsMessage');
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    languageMenu = document.createElement('language-menu');
    document.body.appendChild(languageMenu);
    languageMenu.baseLanguages = new Set();
    languageMenu.voicePackInstallStatus = {};
    flush();
  });

  suite('using pack manager languages', () => {
    setup(() => {
      languageMenu.baseLanguages = AVAILABLE_GOOGLE_TTS_LOCALES;
      flush();
    });

    test('with existing available language no duplicates added', () => {
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
      const expectedLanguages =
          chrome.readingMode.isLanguagePackDownloadingEnabled &&
              chrome.readingMode.isChromeOsAsh ?
          34 :
          availableVoices.length;
      setAvailableVoices();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(expectedLanguages, getLanguageLineItems().length);
    });
  });

  suite('using some base languages', () => {
    setup(() => {
      languageMenu.baseLanguages = new Set(['en-us']);
      flush();
    });

    test('with existing available language no duplicates added', () => {
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
      setAvailableVoices();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(1, getLanguageLineItems().length);
    });

    test('adds language from available voice', () => {
      const expectedLanguages =
          chrome.readingMode.isLanguagePackDownloadingEnabled &&
              chrome.readingMode.isChromeOsAsh ?
          1 :
          0;
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 5', lang: 'en-es'})];
      setAvailableVoices();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(expectedLanguages + 1, getLanguageLineItems().length);
    });

    test('sorts alphabetically', () => {
      const expectedLanguages =
          chrome.readingMode.isLanguagePackDownloadingEnabled &&
              chrome.readingMode.isChromeOsAsh ?
          1 :
          0;
      availableVoices = [
        createSpeechSynthesisVoice({name: 'Steve', lang: 'da-dk'}),
        createSpeechSynthesisVoice({name: 'Dustin', lang: 'bn-bd'}),
      ];
      setAvailableVoices();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(expectedLanguages + 2, getLanguageLineItems().length);
      assertLanguageLineWithTextAndSwitch('bn-bd', getLanguageLineItems()[0]!);
      assertLanguageLineWithTextAndSwitch('da-dk', getLanguageLineItems()[1]!);
    });
  });

  suite('with one language', () => {
    setup(() => {
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
      setAvailableVoices();
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        () => {
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(1, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'en-us', getLanguageLineItems()[0]!);
          assertEquals('', getLanguageSearchField().value);
        });

    test('when availableVoices updates menu displays the new languages', () => {
      availableVoices = [
        createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'}),
        createSpeechSynthesisVoice({name: 'test voice 2', lang: 'en-UK'}),
      ];
      setAvailableVoices();

      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(2, getLanguageLineItems().length);
      assertLanguageLineWithTextAndSwitch('en-uk', getLanguageLineItems()[0]!);
      assertLanguageLineWithTextAndSwitch('en-us', getLanguageLineItems()[1]!);
      assertEquals('', getLanguageSearchField().value);
      assertEquals(true, getNoResultsFoundMessage()!.hidden);
    });

    suite('with display names for locales', () => {
      setup(() => {
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
        };
        flush();
      });

      test('it displays the display name', () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[0]!);
      });

      suite('with search input', () => {
        test('it displays no language without a match', async () => {
          getLanguageSearchField().value = 'test';
          await getLanguageSearchField().updateComplete;
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(0, getLanguageLineItems().length);
          assertEquals(false, getNoResultsFoundMessage()!.hidden);
        });

        test('it displays matching language with a match', async () => {
          getLanguageSearchField().value = 'english';
          await getLanguageSearchField().updateComplete;
          assertEquals(1, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'English (United States)', getLanguageLineItems()[0]!);
          assertEquals(true, getNoResultsFoundMessage()!.hidden);
        });
      });
    });
  });

  suite('with multiple languages', () => {
    setup(() => {
      availableVoices = [
        createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
        createSpeechSynthesisVoice({name: 'test voice 1', lang: 'it-IT'}),
        createSpeechSynthesisVoice({name: 'test voice 2', lang: 'en-UK'}),
      ];
      setAvailableVoices();
      enabledLangs = ['Italian'];
      setEnabledLanguages();
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        () => {
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(3, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'en-uk', getLanguageLineItems()[0]!);
          assertLanguageLineWithTextAndSwitch(
              'en-us', getLanguageLineItems()[1]!);
          assertLanguageLineWithTextAndSwitch(
              'it-it', getLanguageLineItems()[2]!);
          assertEquals('', getLanguageSearchField().value);
        });

    suite('with display names for locales', () => {
      setup(() => {
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
          'it-it': 'Italian',
          'en-uk': 'English (United Kingdom)',
        };
        flush();
      });

      test('it displays the display name', () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United Kingdom)', getLanguageLineItems()[0]!);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[1]!);
        assertLanguageLineWithTextAndSwitch(
            'Italian', getLanguageLineItems()[2]!);
        assertEquals('', getLanguageSearchField().value);
      });

      test('it does not groups languages with different name', () => {
        availableVoices = [
          createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
          createSpeechSynthesisVoice({name: 'test voice 3', lang: 'en'}),
        ];
        setAvailableVoices();
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
          'en': 'English',
        };
        flush();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(2, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English', getLanguageLineItems()[0]!);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[1]!);
      });

      test('it toggles switch on for initially enabled line', async () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[0]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[1]!);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[2]!);
      });

      test('it toggles switch when language pref changes', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        setEnabledLanguages();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[0]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[1]!);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[2]!);
      });

      test('it shows no notification initially', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        setEnabledLanguages();
        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification('', getNotificationItems()[2]!);
      });

      test('it shows and hides downloading notification', async () => {
        languageMenu.baseLanguages = new Set(['it-it']);
        enabledLangs = ['it-it', 'English (United States)'];
        setEnabledLanguages();
        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST;
        setNotificationForLanguage();
        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification(
            'Downloading voices…', getNotificationItems()[2]!);

        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE;
        setNotificationForLanguage();
        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification(
            'Downloading voices…', getNotificationItems()[2]!);

        languagesToNotificationMap['it'] = VoiceClientSideStatusCode.AVAILABLE;
        setNotificationForLanguage();
        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification('', getNotificationItems()[2]!);
      });


      test('non-Google language does not show downloading notification', () => {
        languageMenu.baseLanguages = new Set(['it', 'en-us']);
        enabledLangs = ['it', 'en-us', 'es'];
        setEnabledLanguages();

        availableVoices = [
          createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-us'}),
          createSpeechSynthesisVoice({name: 'espeak voice', lang: 'es'}),
        ];
        setAvailableVoices();
        languagesToNotificationMap['es'] =
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST;
        setNotificationForLanguage();

        assertEquals(2, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
      });

      test('shows generic error notification with internet', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        setEnabledLanguages();
        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.ERROR_INSTALLING;
        setNotificationForLanguage();
        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification(
            'Download failed', getNotificationItems()[2]!);
      });

      test('does not show old error notifications', () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const newMenu = document.createElement('language-menu');
        newMenu.voicePackInstallStatus = {
          'it': VoiceClientSideStatusCode.ERROR_INSTALLING,
        };
        newMenu.availableVoices = [
          createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
          createSpeechSynthesisVoice({name: 'test voice 1', lang: 'it-IT'}),
          createSpeechSynthesisVoice({name: 'test voice 2', lang: 'en-UK'}),
        ];
        document.body.appendChild(newMenu);
        flush();

        const notificationItems =
            newMenu.$.languageMenu.querySelectorAll<HTMLElement>(
                '#notificationText');

        assertEquals(3, notificationItems.length);
        assertLanguageNotification('', notificationItems[0]!);
        assertLanguageNotification('', notificationItems[1]!);
        assertLanguageNotification('', notificationItems[2]!);
      });

      test('shows old downloading notifications', () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const newMenu = document.createElement('language-menu');
        newMenu.voicePackInstallStatus = {
          'it': VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
        };
        newMenu.availableVoices = [
          createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
          createSpeechSynthesisVoice({name: 'test voice 1', lang: 'it-IT'}),
          createSpeechSynthesisVoice({name: 'test voice 2', lang: 'en-UK'}),
        ];
        document.body.appendChild(newMenu);
        flush();

        const notificationItems =
            newMenu.$.languageMenu.querySelectorAll<HTMLElement>(
                '#notificationText');

        assertEquals(3, notificationItems.length);
        assertLanguageNotification('', notificationItems[0]!);
        assertLanguageNotification('', notificationItems[1]!);
        assertLanguageNotification(
            'Downloading voices…', notificationItems[2]!);
      });

      test(
          'shows high quality allocation notification', () => {
            enabledLangs = ['Italian', 'English (United States)'];
            setEnabledLanguages();
            languagesToNotificationMap['it'] =
                VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION;
            setNotificationForLanguage();
            assertEquals(3, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
            assertLanguageNotification(
                'For higher quality voices, clear space on your device',
                getNotificationItems()[2]!);
          });

      test('with no voices it shows allocation notification ', async () => {
        languageMenu.baseLanguages = new Set(['it', 'English (United States)']);

        enabledLangs = ['it', 'English (United States)'];
        setEnabledLanguages();

        availableVoices =
            [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
        setAvailableVoices();

        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION;
        setNotificationForLanguage();

        // Languages without an already installed voice are not available
        // when on another platform than ChromeOS Ash and when the language
        // pack downloading flag is disabled. Therefore, it won't be possible
        // to test the non-high quality voice allocation error message when
        // languages for uninstalled languages are unavailable.
        const areLanguagesWithUninstalledVoicesAvailable =
            chrome.readingMode.isChromeOsAsh &&
            chrome.readingMode.isLanguagePackDownloadingEnabled;
        const notificationItemSize =
            areLanguagesWithUninstalledVoicesAvailable ? 3 : 1;
        assertEquals(notificationItemSize, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);

        if (notificationItemSize > 1) {
          assertLanguageNotification('', getNotificationItems()[1]!);
          assertLanguageNotification(
              'To install this language, clear space on your device',
              getNotificationItems()[2]!);
        }
      });

      suite('with search input', () => {
        test('it displays no language without a match', async () => {
          getLanguageSearchField().value = 'test';
          await getLanguageSearchField().updateComplete;
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(0, getLanguageLineItems().length);
        });

        test('it displays matching language with a match', async () => {
          getLanguageSearchField().value = 'italian';
          await getLanguageSearchField().updateComplete;
          assertEquals(1, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'Italian', getLanguageLineItems()[0]!);
        });
      });
    });
  });

  suite('with multiple voices for the same language', () => {
    setup(() => {
      availableVoices = [
        createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
        createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'}),
        createSpeechSynthesisVoice({name: 'test voice 2', lang: 'en-UK'}),
        createSpeechSynthesisVoice({name: 'test voice 3', lang: 'en-UK'}),
        createSpeechSynthesisVoice({name: 'test voice 4', lang: 'it-IT'}),
        createSpeechSynthesisVoice({name: 'test voice 5', lang: 'zh-CN'}),
      ];
      setAvailableVoices();
    });

    test('only shows one line per unique language name', () => {
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(4, getLanguageLineItems().length);
      assertLanguageLineWithTextAndSwitch('en-uk', getLanguageLineItems()[0]!);
      assertLanguageLineWithTextAndSwitch('en-us', getLanguageLineItems()[1]!);
      assertLanguageLineWithTextAndSwitch('it-it', getLanguageLineItems()[2]!);
      assertLanguageLineWithTextAndSwitch('zh-cn', getLanguageLineItems()[3]!);
    });

    suite('with display names for locales', () => {
      setup(() => {
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
          'it-it': 'Italian',
          'en-uk': 'English (United Kingdom)',
          'zh-cn': 'Chinese',
        };
        flush();
      });

      test('it displays the display name', () => {
        flush();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(4, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'Chinese', getLanguageLineItems()[0]!);
        assertLanguageLineWithTextAndSwitch(
            'English (United Kingdom)', getLanguageLineItems()[1]!);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[2]!);
        assertLanguageLineWithTextAndSwitch(
            'Italian', getLanguageLineItems()[3]!);
        assertEquals('', getLanguageSearchField().value);
      });

      suite('with search input', () => {
        test('it displays no language without a match', async () => {
          getLanguageSearchField().value = 'test';
          await getLanguageSearchField().updateComplete;
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(0, getLanguageLineItems().length);
        });

        test('it displays matching language with a match', async () => {
          getLanguageSearchField().value = 'chin';
          await getLanguageSearchField().updateComplete;
          assertEquals(1, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'Chinese', getLanguageLineItems()[0]!);
        });
      });
    });
  });
});

function isPositionedOnPage(element: HTMLElement) {
  return !!element &&
      !!(element.offsetWidth || element.offsetHeight ||
         element.getClientRects().length);
}

function assertLanguageLineWithTextAndSwitch(
    expectedText: string, element: HTMLElement) {
  assertEquals(expectedText, element.textContent!.trim());
  assertEquals(2, element.children.length);
  assertEquals('CR-TOGGLE', element.children[1]!.tagName);
}

async function assertLanguageLineWithToggleChecked(
    expectedChecked: boolean, element: HTMLElement) {
  const toggle: CrToggleElement = (element.querySelector('cr-toggle'))!;
  await toggle.updateComplete;
  if (expectedChecked) {
    assertTrue(toggle.checked);
    assertTrue(toggle.hasAttribute('checked'));
    assertEquals('true', toggle.getAttribute('aria-pressed'));
  } else {
    assertFalse(toggle.checked);
    assertEquals(null, toggle.getAttribute('checked'));
    assertEquals('false', toggle.getAttribute('aria-pressed'));
  }
}

function assertLanguageNotification(
    expectedNotification: string, element: HTMLElement) {
  assertEquals(expectedNotification, element.innerText);
}
