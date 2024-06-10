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

suite('LanguageMenu', () => {
  let languageMenu: LanguageMenuElement;
  let availableVoices: SpeechSynthesisVoice[];
  let enabledLanguagesInPref: string[];
  const languagesToNotificationMap:
      {[language: string]: VoiceClientSideStatusCode} = {};


  const setAvailableVoices = () => {
    // Bypass Typescript compiler to allow us to set a private readonly
    // property
    // @ts-ignore
    languageMenu.availableVoices = availableVoices;
    flush();
  };

  const setEnabledLanguages = () => {
    // Bypass Typescript compiler to allow us to set a private readonly
    // property
    // @ts-ignore
    languageMenu.enabledLanguagesInPref = enabledLanguagesInPref;
    flush();
  };

  const setNotificationForLanguage = () => {
    // Bypass Typescript compiler to allow us to set a private readonly
    // property
    // @ts-ignore
    languageMenu.voicePackInstallStatus = {...languagesToNotificationMap};
    flush();
  };

  const reopenLanguageMenu = () => {
    languageMenu.dispatchEvent(new CustomEvent('cr-dialog-open'));
    flush();
  };

  const getLanguageLineItems = () => {
    return languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
        '.language-line');
  };

  const getNotificationItems = () => {
    return languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
        '#notificationText');
  };

  const getLanguageSearchField = () => {
    return languageMenu.$.languageMenu.querySelector<CrInputElement>(
        '.search-field')!;
  };

  const getNoResultsFoundMessage = () => {
    return languageMenu.$.languageMenu.querySelector<HTMLElement>(
        '#noResultsMessage');
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    languageMenu = document.createElement('language-menu');
    document.body.appendChild(languageMenu);
    // @ts-ignore
    languageMenu.baseLanguages = {};
    // @ts-ignore
    languageMenu.voicePackInstallStatus = {};
    flush();
  });

  suite('using pack manager languages', () => {
    setup(() => {
      // @ts-ignore
      languageMenu.baseLanguages = AVAILABLE_GOOGLE_TTS_LOCALES;
      flush();
    });

    test('with existing available language no duplicates added', () => {
      availableVoices =
          [{name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice];
      const expectedLanguages =
          chrome.readingMode.isLanguagePackDownloadingEnabled &&
              chrome.readingMode.isChromeOsAsh ?
          34 :
          availableVoices.length;
      setAvailableVoices();
      languageMenu.showDialog();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(getLanguageLineItems().length, expectedLanguages);
    });
  });

  suite('using some base languages', () => {
    setup(() => {
      // @ts-ignore
      languageMenu.baseLanguages = ['en-us'];
      flush();
    });

    test('with existing available language no duplicates added', () => {
      availableVoices =
          [{name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice];
      setAvailableVoices();
      languageMenu.showDialog();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(getLanguageLineItems().length, 1);
    });

    test('adds language from available voice', () => {
      const expectedLanguages =
          chrome.readingMode.isLanguagePackDownloadingEnabled &&
              chrome.readingMode.isChromeOsAsh ?
          1 :
          0;
      availableVoices =
          [{name: 'test voice 5', lang: 'en-es'} as SpeechSynthesisVoice];
      setAvailableVoices();
      languageMenu.showDialog();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(getLanguageLineItems().length, expectedLanguages + 1);
    });

    test('sorts alphabetically', () => {
      const expectedLanguages =
          chrome.readingMode.isLanguagePackDownloadingEnabled &&
              chrome.readingMode.isChromeOsAsh ?
          1 :
          0;
      availableVoices = [
        {name: 'Steve', lang: 'da-dk'} as SpeechSynthesisVoice,
        {name: 'Dustin', lang: 'bn-bd'} as SpeechSynthesisVoice,
      ];
      setAvailableVoices();
      languageMenu.showDialog();
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(getLanguageLineItems().length, expectedLanguages + 2);
      assertLanguageLineWithTextAndSwitch(getLanguageLineItems()[0]!, 'bn-bd');
      assertLanguageLineWithTextAndSwitch(getLanguageLineItems()[1]!, 'da-dk');
    });
  });

  suite('with one language', () => {
    setup(() => {
      availableVoices =
          [{name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice];
      setAvailableVoices();
      languageMenu.showDialog();
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        () => {
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(getLanguageLineItems().length, 1);
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[0]!, 'en-US');
          assertEquals(getLanguageSearchField().value, '');
        });

    suite('when availableVoices updates', () => {
      setup(() => {
        availableVoices = [
          {name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice,
          {name: 'test voice 2', lang: 'en-UK'} as SpeechSynthesisVoice,
        ];
        setAvailableVoices();
      });

      test('it updates and displays the new languages', () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 2);
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[0]!, 'en-UK');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[1]!, 'en-US');
        assertEquals(getLanguageSearchField().value, '');
        assertEquals(getNoResultsFoundMessage()!.hidden, true);
      });
    });

    suite('with display names for locales', () => {
      setup(() => {
        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        languageMenu.localeToDisplayName = {
          'en-US': 'English (United States)',
        };
        flush();
      });

      test('it displays the display name', () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 1);
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[0]!, 'English (United States)');
      });

      suite('with search input', () => {
        test('it displays no language without a match', async () => {
          getLanguageSearchField().value = 'test';
          await getLanguageSearchField().updateComplete;
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(getLanguageLineItems().length, 0);
          assertEquals(getNoResultsFoundMessage()!.hidden, false);
        });

        test('it displays matching language with a match', async () => {
          getLanguageSearchField().value = 'english';
          await getLanguageSearchField().updateComplete;
          assertEquals(getLanguageLineItems().length, 1);
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[0]!, 'English (United States)');
          assertEquals(getNoResultsFoundMessage()!.hidden, true);
        });
      });
    });
  });

  suite('with multiple languages', () => {
    setup(() => {
      availableVoices = [
        {name: 'test voice 0', lang: 'en-US'} as SpeechSynthesisVoice,
        {name: 'test voice 1', lang: 'it-IT'} as SpeechSynthesisVoice,
        {name: 'test voice 2', lang: 'en-UK'} as SpeechSynthesisVoice,
      ];
      setAvailableVoices();
      enabledLanguagesInPref = ['Italian'];
      setEnabledLanguages();
      languageMenu.showDialog();
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        () => {
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(getLanguageLineItems().length, 3);
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[0]!, 'en-UK');
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[1]!, 'en-US');
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[2]!, 'it-IT');
          assertEquals(getLanguageSearchField().value, '');
        });

    suite('with display names for locales', () => {
      setup(() => {
        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        languageMenu.localeToDisplayName = {
          'en-US': 'English (United States)',
          'it-IT': 'Italian',
          'en-UK': 'English (United Kingdom)',
        };
        flush();
      });

      test('it displays the display name', () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 3);
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[0]!, 'English (United Kingdom)');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[1]!, 'English (United States)');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[2]!, 'Italian');
        assertEquals(getLanguageSearchField().value, '');
      });

      test('it does not groups languages with different name', () => {
        availableVoices = [
          {name: 'test voice 0', lang: 'en-US'} as SpeechSynthesisVoice,
          {name: 'test voice 3', lang: 'en'} as SpeechSynthesisVoice,
        ];
        setAvailableVoices();
        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        languageMenu.localeToDisplayName = {
          'en-US': 'English (United States)',
          'en': 'English',
        };
        flush();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 2);
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[0]!, 'English');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[1]!, 'English (United States)');
      });

      test('it toggles switch on for initially enabled line', async () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 3);
        assertLanguageLineWithToggleChecked(getLanguageLineItems()[0]!, false);
        assertLanguageLineWithToggleChecked(getLanguageLineItems()[1]!, true);
        assertLanguageLineWithToggleChecked(getLanguageLineItems()[2]!, false);
      });

      test('it toggles switch when language pref changes', async () => {
        enabledLanguagesInPref = ['Italian', 'English (United States)'];
        setEnabledLanguages();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 3);
        assertLanguageLineWithToggleChecked(getLanguageLineItems()[0]!, true);
        assertLanguageLineWithToggleChecked(getLanguageLineItems()[1]!, true);
        assertLanguageLineWithToggleChecked(getLanguageLineItems()[2]!, false);
      });

      test('it shows no notification initially', async () => {
        enabledLanguagesInPref = ['Italian', 'English (United States)'];
        setEnabledLanguages();
        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(getNotificationItems()[2]!, '');
      });

      test('it shows and hides downloading notification', async () => {
        // @ts-ignore
        languageMenu.baseLanguages = ['it-it'];
        enabledLanguagesInPref = ['it-it', 'English (United States)'];
        setEnabledLanguages();
        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST;
        setNotificationForLanguage();
        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(
            getNotificationItems()[2]!, 'Downloading voices…');

        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE;
        setNotificationForLanguage();
        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(
            getNotificationItems()[2]!, 'Downloading voices…');

        languagesToNotificationMap['it'] = VoiceClientSideStatusCode.AVAILABLE;
        setNotificationForLanguage();
        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(getNotificationItems()[2]!, '');
      });

      test('hides downloading notification after a reopen', async () => {
        // @ts-ignore
        languageMenu.baseLanguages = ['it-it'];
        enabledLanguagesInPref = ['it-it', 'English (United States)'];
        setEnabledLanguages();
        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST;
        setNotificationForLanguage();

        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(
            getNotificationItems()[2]!, 'Downloading voices…');

        reopenLanguageMenu();
        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(
            getNotificationItems()[2]!, 'Downloading voices…');
      });

      test('non-Google language does not show downloading notification', () => {
        // @ts-ignore
        languageMenu.baseLanguages = ['it', 'en-us'];
        enabledLanguagesInPref = ['it', 'en-us', 'es'];
        setEnabledLanguages();

        availableVoices = [
          {name: 'test voice 1', lang: 'en-us'} as SpeechSynthesisVoice,
          {name: 'espeak voice', lang: 'es'} as SpeechSynthesisVoice,
        ];
        setAvailableVoices();
        languagesToNotificationMap['es'] =
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST;
        setNotificationForLanguage();

        assertEquals(getNotificationItems().length, 2);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
      });

      test('shows generic error notification with internet', async () => {
        enabledLanguagesInPref = ['Italian', 'English (United States)'];
        setEnabledLanguages();
        languagesToNotificationMap['it'] =
            VoiceClientSideStatusCode.ERROR_INSTALLING;
        setNotificationForLanguage();
        assertEquals(getNotificationItems().length, 3);
        assertLanguageNotification(getNotificationItems()[0]!, '');
        assertLanguageNotification(getNotificationItems()[1]!, '');
        assertLanguageNotification(
            getNotificationItems()[2]!, 'Download failed');
      });


      test(
          'with other voices it shows high quality allocation notification',
          async () => {
            enabledLanguagesInPref = ['Italian', 'English (United States)'];
            setEnabledLanguages();
            languagesToNotificationMap['it'] =
                VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION;
            setNotificationForLanguage();
            assertEquals(getNotificationItems().length, 3);
            assertLanguageNotification(getNotificationItems()[0]!, '');
            assertLanguageNotification(getNotificationItems()[1]!, '');
            assertLanguageNotification(
                getNotificationItems()[2]!,
                'For higher quality voices, clear space on your device');
          });

      test(
          'high quality allocation notification cleared after reopen',
          async () => {
            enabledLanguagesInPref = ['Italian', 'English (United States)'];
            setEnabledLanguages();
            languagesToNotificationMap['it'] =
                VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION;
            setNotificationForLanguage();

            assertEquals(getNotificationItems().length, 3);
            assertLanguageNotification(getNotificationItems()[0]!, '');
            assertLanguageNotification(getNotificationItems()[1]!, '');
            assertLanguageNotification(
                getNotificationItems()[2]!,
                'For higher quality voices, clear space on your device');

            reopenLanguageMenu();
            assertEquals(getNotificationItems().length, 3);
            assertLanguageNotification(getNotificationItems()[0]!, '');
            assertLanguageNotification(getNotificationItems()[1]!, '');
            assertLanguageNotification(getNotificationItems()[2]!, '');
          });

      test('with no voices it shows allocation notification ', async () => {
        // @ts-ignore
        languageMenu.baseLanguages = ['it', 'English (United States)'];

        enabledLanguagesInPref = ['it', 'English (United States)'];
        setEnabledLanguages();

        availableVoices =
            [{name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice];
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
        assertEquals(getNotificationItems().length, notificationItemSize);
        assertLanguageNotification(getNotificationItems()[0]!, '');

        if (notificationItemSize > 1) {
          assertLanguageNotification(getNotificationItems()[1]!, '');
          assertLanguageNotification(
              getNotificationItems()[2]!,
              'To install this language, clear space on your device');
        }
      });

      test('allocation notification cleared after reopen', async () => {
        // @ts-ignore
        languageMenu.baseLanguages = ['it', 'English (United States)'];

        enabledLanguagesInPref = ['it', 'English (United States)'];
        setEnabledLanguages();

        availableVoices =
            [{name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice];
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
        assertEquals(getNotificationItems().length, notificationItemSize);
        assertLanguageNotification(getNotificationItems()[0]!, '');

        if (notificationItemSize > 1) {
          assertLanguageNotification(getNotificationItems()[1]!, '');
          assertLanguageNotification(
              getNotificationItems()[2]!,
              'To install this language, clear space on your device');
        }

        reopenLanguageMenu();

        // Assert that the notification has cleared.
        if (notificationItemSize > 1) {
          assertLanguageNotification(getNotificationItems()[1]!, '');
          assertLanguageNotification(getNotificationItems()[2]!, '');
        }
      });

      suite('with search input', () => {
        test('it displays no language without a match', async () => {
          getLanguageSearchField().value = 'test';
          await getLanguageSearchField().updateComplete;
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(getLanguageLineItems().length, 0);
        });

        test('it displays matching language with a match', async () => {
          getLanguageSearchField().value = 'italian';
          await getLanguageSearchField().updateComplete;
          assertEquals(getLanguageLineItems().length, 1);
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[0]!, 'Italian');
        });
      });
    });
  });

  suite('with multiple voices for the same language', () => {
    setup(() => {
      availableVoices = [
        {name: 'test voice 0', lang: 'en-US'} as SpeechSynthesisVoice,
        {name: 'test voice 1', lang: 'en-US'} as SpeechSynthesisVoice,
        {name: 'test voice 2', lang: 'en-UK'} as SpeechSynthesisVoice,
        {name: 'test voice 3', lang: 'en-UK'} as SpeechSynthesisVoice,
        {name: 'test voice 4', lang: 'it-IT'} as SpeechSynthesisVoice,
        {name: 'test voice 5', lang: 'zh-CN'} as SpeechSynthesisVoice,
      ];
      setAvailableVoices();
      languageMenu.showDialog();
    });

    test('only shows one line per unique language name', () => {
      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(getLanguageLineItems().length, 4);
      assertLanguageLineWithTextAndSwitch(getLanguageLineItems()[0]!, 'en-UK');
      assertLanguageLineWithTextAndSwitch(getLanguageLineItems()[1]!, 'en-US');
      assertLanguageLineWithTextAndSwitch(getLanguageLineItems()[2]!, 'it-IT');
      assertLanguageLineWithTextAndSwitch(getLanguageLineItems()[3]!, 'zh-CN');
    });

    suite('with display names for locales', () => {
      setup(() => {
        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        languageMenu.localeToDisplayName = {
          'en-US': 'English (United States)',
          'it-IT': 'Italian',
          'en-UK': 'English (United Kingdom)',
          'zh-CN': 'Chinese',
        };
        flush();
      });

      test('it displays the display name', () => {
        flush();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(getLanguageLineItems().length, 4);
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[0]!, 'Chinese');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[1]!, 'English (United Kingdom)');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[2]!, 'English (United States)');
        assertLanguageLineWithTextAndSwitch(
            getLanguageLineItems()[3]!, 'Italian');
        assertEquals(getLanguageSearchField().value, '');
      });

      suite('with search input', () => {
        test('it displays no language without a match', async () => {
          getLanguageSearchField().value = 'test';
          await getLanguageSearchField().updateComplete;
          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(getLanguageLineItems().length, 0);
        });

        test('it displays matching language with a match', async () => {
          getLanguageSearchField().value = 'chin';
          await getLanguageSearchField().updateComplete;
          assertEquals(getLanguageLineItems().length, 1);
          assertLanguageLineWithTextAndSwitch(
              getLanguageLineItems()[0]!, 'Chinese');
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
    element: HTMLElement, expectedText: string) {
  assertEquals(expectedText, element.textContent!.trim());
  assertEquals(2, element.children.length);
  assertEquals('CR-TOGGLE', element.children[1]!.tagName);
}

async function assertLanguageLineWithToggleChecked(
    element: HTMLElement, expectedChecked: boolean) {
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
    element: HTMLElement, expectedNotification: string) {
  assertEquals(element.innerText, expectedNotification);
}
