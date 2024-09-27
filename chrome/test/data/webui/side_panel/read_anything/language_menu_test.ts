// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import type {LanguageMenuElement, LanguageToastElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, VoiceClientSideStatusCode, VoiceNotificationManager} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice} from './common.js';

suite('LanguageMenu', () => {
  let languageMenu: LanguageMenuElement;
  let availableVoices: SpeechSynthesisVoice[];
  let enabledLangs: string[];

  function getLanguageLineItems() {
    return languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
        '.language-line');
  }

  function getNotificationItems() {
    return languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
        '#notificationText');
  }

  function getToast(): LanguageToastElement {
    const toast =
        languageMenu.$.languageMenu.querySelector<LanguageToastElement>(
            'language-toast');
    assertTrue(!!toast);
    return toast;
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
    VoiceNotificationManager.getInstance().clear();
    languageMenu = document.createElement('language-menu');
    languageMenu.localesOfLangPackVoices = new Set(['it-it']);
  });

  test('with existing available language no duplicates added', () => {
    availableVoices =
        [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
    languageMenu.availableVoices = availableVoices;
    languageMenu.localesOfLangPackVoices = AVAILABLE_GOOGLE_TTS_LOCALES;
    document.body.appendChild(languageMenu);

    assertTrue(isPositionedOnPage(languageMenu));
    assertEquals(34, getLanguageLineItems().length);
  });

  suite('using some base languages', () => {
    setup(() => {
      languageMenu.localesOfLangPackVoices = new Set(['en-us']);
    });

    test('with existing available language no duplicates added', () => {
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
      languageMenu.availableVoices = availableVoices;
      document.body.appendChild(languageMenu);

      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(1, getLanguageLineItems().length);
    });

    test('adds language from available voice', () => {
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 5', lang: 'en-es'})];
      languageMenu.availableVoices = availableVoices;
      document.body.appendChild(languageMenu);

      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(2, getLanguageLineItems().length);
    });

    test('sorts alphabetically', () => {
      availableVoices = [
        createSpeechSynthesisVoice({name: 'Steve', lang: 'da-dk'}),
        createSpeechSynthesisVoice({name: 'Dustin', lang: 'bn-bd'}),
      ];
      languageMenu.availableVoices = availableVoices;
      document.body.appendChild(languageMenu);

      assertTrue(isPositionedOnPage(languageMenu));
      assertEquals(3, getLanguageLineItems().length);
      assertLanguageLineWithTextAndSwitch('bn-bd', getLanguageLineItems()[0]!);
      assertLanguageLineWithTextAndSwitch('da-dk', getLanguageLineItems()[1]!);
    });
  });

  suite('with one language', () => {
    setup(() => {
      languageMenu.localesOfLangPackVoices = new Set(['en-us']);
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
      languageMenu.availableVoices = availableVoices;
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        () => {
          document.body.appendChild(languageMenu);

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
      languageMenu.availableVoices = availableVoices;
      document.body.appendChild(languageMenu);

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
      });

      test('it displays the display name', () => {
        document.body.appendChild(languageMenu);

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[0]!);
      });

      test('it displays no language without a match', async () => {
        document.body.appendChild(languageMenu);
        getLanguageSearchField().value = 'test';
        await microtasksFinished();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(0, getLanguageLineItems().length);
        assertEquals(false, getNoResultsFoundMessage()!.hidden);
      });

      test('it displays matching language with a match', async () => {
        document.body.appendChild(languageMenu);
        getLanguageSearchField().value = 'english';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[0]!);
        assertEquals(true, getNoResultsFoundMessage()!.hidden);
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
      languageMenu.availableVoices = availableVoices;
      enabledLangs = ['Italian'];
      languageMenu.enabledLangs = enabledLangs;
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        () => {
          document.body.appendChild(languageMenu);

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
      function notify(language: string, status: VoiceClientSideStatusCode) {
        VoiceNotificationManager.getInstance().onVoiceStatusChange(
            language, status, availableVoices);
      }

      setup(() => {
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
          'it-it': 'Italian',
          'en-uk': 'English (United Kingdom)',
        };
      });

      test('it displays the display name', () => {
        document.body.appendChild(languageMenu);

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

      test('it does not group languages with different names', () => {
        languageMenu.localesOfLangPackVoices = new Set(['en-us']);
        availableVoices = [
          createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
          createSpeechSynthesisVoice({name: 'test voice 3', lang: 'en'}),
        ];
        languageMenu.availableVoices = availableVoices;
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
          'en': 'English',
        };
        document.body.appendChild(languageMenu);

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(2, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English', getLanguageLineItems()[0]!);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[1]!);
      });

      test('it toggles switch on for initially enabled line', () => {
        document.body.appendChild(languageMenu);

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[0]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[1]!);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[2]!);
      });

      test('it toggles switch when language pref changes', () => {
        enabledLangs = ['Italian', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        document.body.appendChild(languageMenu);

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[0]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[1]!);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[2]!);
      });

      test('it shows no notification initially', () => {
        enabledLangs = ['Italian', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        document.body.appendChild(languageMenu);

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification('', getNotificationItems()[2]!);
        assertFalse(getToast().$.toast.open);
      });

      // <if expr="chromeos_ash">
      test('it shows downloaded toast', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        document.body.appendChild(languageMenu);
        await microtasksFinished();
        notify('it', VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();

        assertTrue(getToast().$.toast.open);
      });

      test('it does not show error toast', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        document.body.appendChild(languageMenu);
        await microtasksFinished();
        notify('it', VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
        await microtasksFinished();

        assertFalse(getToast().$.toast.open);
      });

      test('it does not show downloaded toast when closed', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        document.body.appendChild(languageMenu);
        await microtasksFinished();
        const closeButton =
            languageMenu.$.languageMenu.querySelector<HTMLElement>('#close');
        closeButton!.click();
        await microtasksFinished();
        notify('it', VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
        await microtasksFinished();

        assertFalse(getToast().$.toast.open);
      });
      // </if>

      test('it shows and hides downloading notification', async () => {
        languageMenu.localesOfLangPackVoices = new Set(['it-it']);
        enabledLangs = ['it-it', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        document.body.appendChild(languageMenu);
        await microtasksFinished();

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification(
            'Downloading voices…', getNotificationItems()[2]!);

        notify('it', VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
        await microtasksFinished();

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification(
            'Downloading voices…', getNotificationItems()[2]!);

        notify('it', VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification('', getNotificationItems()[2]!);
      });

      test(
          'non-Google language does not show downloading notification',
          async () => {
            languageMenu.localesOfLangPackVoices = new Set(['en-us']);
            enabledLangs = ['it', 'en-us', 'es'];
            languageMenu.enabledLangs = enabledLangs;
            availableVoices = [
              createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-us'}),
              createSpeechSynthesisVoice({name: 'espeak voice', lang: 'es'}),
            ];
            languageMenu.availableVoices = availableVoices;
            notify('es', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
            document.body.appendChild(languageMenu);
            await microtasksFinished();

            assertEquals(2, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
          });

      test(
          'shows generic error notification with internet and no other voices for this language',
          async () => {
            enabledLangs = ['Italian', 'English (United States)'];
            // Remove the italian voice so we can test when there's no voices
            // for this language.
            availableVoices = availableVoices.filter(v => v.lang !== 'it-IT');
            languageMenu.availableVoices = availableVoices;
            languageMenu.enabledLangs = enabledLangs;
            document.body.appendChild(languageMenu);
            notify('it', VoiceClientSideStatusCode.ERROR_INSTALLING);
            await microtasksFinished();

            assertEquals(3, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
            assertLanguageNotification(
                'Download failed', getNotificationItems()[2]!);
          });

      test(
          'shows no error notification when other voices for this language are available',
          async () => {
            enabledLangs = ['Italian', 'English (United States)'];
            languageMenu.enabledLangs = enabledLangs;
            document.body.appendChild(languageMenu);
            notify('it', VoiceClientSideStatusCode.ERROR_INSTALLING);
            await microtasksFinished();

            assertEquals(3, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
            assertLanguageNotification('', getNotificationItems()[2]!);
          });

      test('does not show old error notifications', async () => {
        notify('it', VoiceClientSideStatusCode.ERROR_INSTALLING);
        document.body.appendChild(languageMenu);
        await microtasksFinished();

        const notificationItems: HTMLElement[] = Array.from(
            languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
                '#notificationText'));

        const noNotifications = notificationItems.every(
            notification => notification.innerText === '');
        assertTrue(noNotifications);
      });

      test('shows old downloading notifications', async () => {
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        document.body.appendChild(languageMenu);
        await microtasksFinished();

        const notificationItems: HTMLElement[] = Array.from(
            languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
                '#notificationText'));

        const downloadingNotifications = notificationItems.filter(
            notification => notification.innerText === 'Downloading voices…');
        assertEquals(1, downloadingNotifications.length);
      });

      test('shows high quality allocation notification', async () => {
        enabledLangs = ['Italian', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        document.body.appendChild(languageMenu);
        notify('it', VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
        await microtasksFinished();

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification(
            'For higher quality voices, clear space on your device',
            getNotificationItems()[2]!);
      });

      test('with no voices it shows allocation notification ', async () => {
        languageMenu.localesOfLangPackVoices =
            new Set(['it', 'English (United States)']);
        enabledLangs = ['it', 'English (United States)'];
        languageMenu.enabledLangs = enabledLangs;
        availableVoices =
            [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
        languageMenu.availableVoices = availableVoices;
        document.body.appendChild(languageMenu);

        notify('it', VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
        await microtasksFinished();

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);

        assertLanguageNotification(
            'To install this language, clear space on your device',
            getNotificationItems()[2]!);
      });

      test('it displays no language without a match', async () => {
        document.body.appendChild(languageMenu);
        getLanguageSearchField().value = 'test';
        await microtasksFinished();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(0, getLanguageLineItems().length);
      });

      test('it displays matching language with a match', async () => {
        document.body.appendChild(languageMenu);
        getLanguageSearchField().value = 'italian';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'Italian', getLanguageLineItems()[0]!);
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
      languageMenu.availableVoices = availableVoices;
    });

    test('only shows one line per unique language name', () => {
      document.body.appendChild(languageMenu);

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
      });

      test('it displays the display name', () => {
        document.body.appendChild(languageMenu);

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

      test('it displays no language without a match', async () => {
        document.body.appendChild(languageMenu);

        getLanguageSearchField().value = 'test';
        await microtasksFinished();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(0, getLanguageLineItems().length);
      });

      test('it displays matching language with a match', async () => {
        document.body.appendChild(languageMenu);

        getLanguageSearchField().value = 'chin';
        await microtasksFinished();
        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'Chinese', getLanguageLineItems()[0]!);
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
