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

  function getLanguageSearchClearButton() {
    return languageMenu.$.languageMenu.querySelector<HTMLElement>(
        '#clearLanguageSearch');
  }

  function getNoResultsFoundMessage() {
    return languageMenu.$.languageMenu.querySelector<HTMLElement>(
        '#noResultsMessage');
  }

  async function drawLanguageMenu(): Promise<void> {
    assertTrue(!!languageMenu);
    await document.body.appendChild(languageMenu);
    return microtasksFinished();
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    VoiceNotificationManager.getInstance().clear();
    languageMenu = document.createElement('language-menu');
    languageMenu.localesOfLangPackVoices = new Set(['it-it']);
  });

  test(
      'with all lang pack voices and existing available language no ' +
          'duplicates added',
      async () => {
        availableVoices =
            [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'it-IT'})];
        languageMenu.availableVoices = availableVoices;
        languageMenu.localesOfLangPackVoices = AVAILABLE_GOOGLE_TTS_LOCALES;
        await drawLanguageMenu();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(
            AVAILABLE_GOOGLE_TTS_LOCALES.size, getLanguageLineItems().length);
      });

  test('with existing available language no duplicates added', async () => {
    availableVoices =
        [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'it-IT'})];
    languageMenu.availableVoices = availableVoices;
    await drawLanguageMenu();

    assertTrue(isPositionedOnPage(languageMenu));
    assertEquals(1, getLanguageLineItems().length);
  });

  test('adds language from available voice', async () => {
    availableVoices =
        [createSpeechSynthesisVoice({name: 'test voice 5', lang: 'en-es'})];
    languageMenu.availableVoices = availableVoices;
    await drawLanguageMenu();

    assertTrue(isPositionedOnPage(languageMenu));
    assertEquals(2, getLanguageLineItems().length);
  });

  test('sorts alphabetically', async () => {
    availableVoices = [
      createSpeechSynthesisVoice({name: 'Steve', lang: 'da-dk'}),
      createSpeechSynthesisVoice({name: 'Dustin', lang: 'bn-bd'}),
    ];
    languageMenu.availableVoices = availableVoices;
    await drawLanguageMenu();

    assertTrue(isPositionedOnPage(languageMenu));
    assertEquals(3, getLanguageLineItems().length);
    assertLanguageLineWithTextAndSwitch('bn-bd', getLanguageLineItems()[0]!);
    assertLanguageLineWithTextAndSwitch('da-dk', getLanguageLineItems()[1]!);
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
        async () => {
          await drawLanguageMenu();

          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(1, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'en-us', getLanguageLineItems()[0]!);
          assertEquals('', getLanguageSearchField().value);
        });

    test(
        'when availableVoices updates menu displays the new languages',
        async () => {
          availableVoices = [
            createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'}),
            createSpeechSynthesisVoice({name: 'test voice 2', lang: 'en-UK'}),
          ];
          languageMenu.availableVoices = availableVoices;
          await drawLanguageMenu();

          assertTrue(isPositionedOnPage(languageMenu));
          assertEquals(2, getLanguageLineItems().length);
          assertLanguageLineWithTextAndSwitch(
              'en-uk', getLanguageLineItems()[0]!);
          assertLanguageLineWithTextAndSwitch(
              'en-us', getLanguageLineItems()[1]!);
          assertEquals('', getLanguageSearchField().value);
          assertEquals(true, getNoResultsFoundMessage()!.hidden);
        });

    suite('with display names for locales', () => {
      setup(() => {
        languageMenu.localeToDisplayName = {
          'en-us': 'English (United States)',
        };
        return drawLanguageMenu();
      });

      test('it displays the display name', () => {
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[0]!);
      });

      test('it displays no language without a match', async () => {
        getLanguageSearchField().value = 'test';
        await microtasksFinished();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(0, getLanguageLineItems().length);
        assertEquals(false, getNoResultsFoundMessage()!.hidden);
      });

      test('it displays matching language with a match', async () => {
        getLanguageSearchField().value = 'english';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[0]!);
        assertEquals(true, getNoResultsFoundMessage()!.hidden);
      });

      test('it matches the language code', async () => {
        getLanguageSearchField().value = 'en-us';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[0]!);
        assertEquals(true, getNoResultsFoundMessage()!.hidden);
      });

      test('shows clear button when search field has contents', async () => {
        getLanguageSearchField().value = 'eng';
        await microtasksFinished();

        assertEquals('eng', getLanguageSearchField().value);

        const clearButton = getLanguageSearchClearButton();
        assertTrue(!!clearButton);
      });

      test(
          'does not show clear button when search field has no content',
          async () => {
            getLanguageSearchField().value = '';
            await microtasksFinished();

            assertEquals('', getLanguageSearchField().value);

            const clearButton = getLanguageSearchClearButton();
            assertFalse(!!clearButton);
          });

      test('clears search field when clear button is clicked', async () => {
        getLanguageSearchField().value = 'xxx';
        await microtasksFinished();

        assertEquals('xxx', getLanguageSearchField().value);

        const clearButton = getLanguageSearchClearButton();
        assertTrue(!!clearButton);

        clearButton.click();
        await microtasksFinished();

        assertEquals('', getLanguageSearchField().value);
      });
    });

    suite('with display names with accent', () => {
      const portugueseDisplayName = 'Português (Brasil)';

      setup(() => {
        availableVoices = [
          createSpeechSynthesisVoice(
              {name: portugueseDisplayName, lang: 'pt-br'}),
        ];
        languageMenu.localeToDisplayName = {
          'pt-br': portugueseDisplayName,
        };
        languageMenu.availableVoices = availableVoices;
        return drawLanguageMenu();
      });

      test('it matches search with accent', async () => {
        getLanguageSearchField().value = 'português';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            portugueseDisplayName, getLanguageLineItems()[0]!);
        assertEquals(true, getNoResultsFoundMessage()!.hidden);
      });

      test('it matches search with no accent', async () => {
        getLanguageSearchField().value = 'portugues';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            portugueseDisplayName, getLanguageLineItems()[0]!);
        assertEquals(true, getNoResultsFoundMessage()!.hidden);
      });

      test('it matches the language code', async () => {
        getLanguageSearchField().value = 'pt-';
        await microtasksFinished();

        assertEquals(1, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            portugueseDisplayName, getLanguageLineItems()[0]!);
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
      enabledLangs = ['it-it'];
      languageMenu.enabledLangs = enabledLangs;
    });

    test(
        'defaults to the locale when there is no display name with a switch',
        async () => {
          await drawLanguageMenu();

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

      test('it displays the display name', async () => {
        await drawLanguageMenu();

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

      test('it does not group languages with different names', async () => {
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
        await drawLanguageMenu();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(2, getLanguageLineItems().length);
        assertLanguageLineWithTextAndSwitch(
            'English', getLanguageLineItems()[0]!);
        assertLanguageLineWithTextAndSwitch(
            'English (United States)', getLanguageLineItems()[1]!);
      });

      test('it toggles switch on for initially enabled line', async () => {
        await drawLanguageMenu();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[0]!);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[1]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[2]!);
      });

      test('it toggles switch when language pref changes', async () => {
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        await drawLanguageMenu();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(3, getLanguageLineItems().length);
        assertLanguageLineWithToggleChecked(false, getLanguageLineItems()[0]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[1]!);
        assertLanguageLineWithToggleChecked(true, getLanguageLineItems()[2]!);
      });

      test('it shows no notification initially', async () => {
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        await drawLanguageMenu();

        assertEquals(3, getNotificationItems().length);
        assertLanguageNotification('', getNotificationItems()[0]!);
        assertLanguageNotification('', getNotificationItems()[1]!);
        assertLanguageNotification('', getNotificationItems()[2]!);
        assertFalse(getToast().$.toast.open);
      });

      // <if expr="is_chromeos">
      test('it shows downloaded toast', async () => {
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        await drawLanguageMenu();
        notify('it', VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();

        assertTrue(getToast().$.toast.open);
      });

      test('it does not show downloaded toast when closed', async () => {
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        await drawLanguageMenu();
        const closeButton =
            languageMenu.$.languageMenu.$.dialog.querySelector<HTMLElement>(
                '#close');
        closeButton!.click();
        await microtasksFinished();
        notify('it', VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
        await microtasksFinished();

        assertFalse(getToast().$.toast.open);
      });
      // </if>

      test('it does not show error toast', async () => {
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        await drawLanguageMenu();
        notify('it', VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
        await microtasksFinished();

        assertFalse(getToast().$.toast.open);
      });

      test('it shows and hides downloading notification', async () => {
        languageMenu.localesOfLangPackVoices = new Set(['it-it']);
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        await drawLanguageMenu();

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
            await drawLanguageMenu();
            await microtasksFinished();

            assertEquals(2, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
          });

      test(
          'shows generic error notification with internet and no other voices' +
              'for this language',
          async () => {
            enabledLangs = ['it-it', 'en-us'];
            // Remove the italian voice so we can test when there's no voices
            // for this language.
            availableVoices = availableVoices.filter(v => v.lang !== 'it-IT');
            languageMenu.availableVoices = availableVoices;
            languageMenu.enabledLangs = enabledLangs;
            await drawLanguageMenu();
            notify('it', VoiceClientSideStatusCode.ERROR_INSTALLING);
            await microtasksFinished();

            assertEquals(3, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
            assertLanguageNotification(
                'Download failed', getNotificationItems()[2]!);
          });

      test(
          'shows no error notification when other voices for this language ' +
              'are available',
          async () => {
            enabledLangs = ['it-it', 'en-us'];
            languageMenu.enabledLangs = enabledLangs;
            await drawLanguageMenu();
            notify('it', VoiceClientSideStatusCode.ERROR_INSTALLING);
            await microtasksFinished();

            assertEquals(3, getNotificationItems().length);
            assertLanguageNotification('', getNotificationItems()[0]!);
            assertLanguageNotification('', getNotificationItems()[1]!);
            assertLanguageNotification('', getNotificationItems()[2]!);
          });

      test('does not show old error notifications', async () => {
        notify('it', VoiceClientSideStatusCode.ERROR_INSTALLING);
        await drawLanguageMenu();

        const notificationItems: HTMLElement[] = Array.from(
            languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
                '#notificationText'));

        const noNotifications = notificationItems.every(
            notification => notification.innerText === '');
        assertTrue(noNotifications);
      });

      test('shows old downloading notifications', async () => {
        notify('it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        await drawLanguageMenu();

        const notificationItems: HTMLElement[] = Array.from(
            languageMenu.$.languageMenu.querySelectorAll<HTMLElement>(
                '#notificationText'));

        const downloadingNotifications = notificationItems.filter(
            notification => notification.innerText === 'Downloading voices…');
        assertEquals(1, downloadingNotifications.length);
      });

      test('shows high quality allocation notification', async () => {
        enabledLangs = ['it-it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        await drawLanguageMenu();
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
        enabledLangs = ['it', 'en-us'];
        languageMenu.enabledLangs = enabledLangs;
        availableVoices =
            [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'})];
        languageMenu.availableVoices = availableVoices;
        await drawLanguageMenu();

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
        await drawLanguageMenu();
        getLanguageSearchField().value = 'test';
        await microtasksFinished();

        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(0, getLanguageLineItems().length);
      });

      test('it displays matching language with a match', async () => {
        await drawLanguageMenu();
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

    test('only shows one line per unique language name', async () => {
      await drawLanguageMenu();

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
        return drawLanguageMenu();
      });

      test('it displays the display name', () => {
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
        getLanguageSearchField().value = 'test';
        await microtasksFinished();
        assertTrue(isPositionedOnPage(languageMenu));
        assertEquals(0, getLanguageLineItems().length);
      });

      test('it displays matching language with a match', async () => {
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
  assertEquals(expectedText, element.textContent.trim());
  assertEquals(2, element.children.length);
  assertEquals('CR-TOGGLE', element.children[1]!.tagName);
}

function assertLanguageLineWithToggleChecked(
    expectedChecked: boolean, element: HTMLElement) {
  const toggle: CrToggleElement = (element.querySelector('cr-toggle'))!;
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
