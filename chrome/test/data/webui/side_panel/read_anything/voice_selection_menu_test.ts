// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {LanguageMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent, VoiceClientSideStatusCode, VoiceNotificationManager} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceSelectionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome-untrusted://webui-test/keyboard_mock_interactions.js';
import {hasStyle, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, stubAnimationFrame} from './common.js';

function stringToHtmlTestId(s: string): string {
  return s.replace(/\s/g, '-').replace(/[()]/g, '');
}

function isPositionedOnPage(element: HTMLElement) {
  return !!element &&
      !!(element.offsetWidth || element.offsetHeight ||
         element.getClientRects().length);
}

suite('VoiceSelectionMenu', () => {
  let voiceSelectionMenu: VoiceSelectionMenuElement;
  let dots: HTMLElement;
  let voice1 =
      createSpeechSynthesisVoice({name: 'Google test voice 1', lang: 'lang'});
  let voice2 =
      createSpeechSynthesisVoice({name: 'Google test voice 2', lang: 'lang'});

  // If no param for enabledLangs is provided, it auto populates it with the
  // langs of the voices
  async function setAvailableVoicesAndEnabledLangs(
      availableVoices: SpeechSynthesisVoice[],
      enabledLangs?: string[]): Promise<void> {
    voiceSelectionMenu.availableVoices = availableVoices;
    if (enabledLangs === undefined) {
      voiceSelectionMenu.enabledLangs =
          [...new Set(availableVoices.map(({lang}) => lang))];
    } else {
      voiceSelectionMenu.enabledLangs = enabledLangs;
    }
    return microtasksFinished();
  }

  function getDropdownItemForVoice(voice: SpeechSynthesisVoice) {
    return voiceSelectionMenu.$.voiceSelectionMenu.get()
        .querySelector<HTMLButtonElement>(`[data-test-id="${
            stringToHtmlTestId(voice.name)}"].dropdown-voice-selection-button`)!
        ;
  }

  function openVoiceMenu(): Promise<void> {
    stubAnimationFrame();
    voiceSelectionMenu.onVoiceSelectionMenuClick(dots);
    return microtasksFinished();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    voiceSelectionMenu = document.createElement('voice-selection-menu');
    document.body.appendChild(voiceSelectionMenu);
    await microtasksFinished();

    // We need an additional call to voiceSelectionMenu.get() to ensure the menu
    // has been rendered.
    voiceSelectionMenu.$.voiceSelectionMenu.get();
    await microtasksFinished();

    // Proxy button as click target to open the menu with
    dots = document.createElement('button');
    const newContent = document.createTextNode('...');
    dots.appendChild(newContent);
    document.body.appendChild(dots);

    return microtasksFinished();
  });

  suite('with one voice', () => {
    setup(() => {
      return setAvailableVoicesAndEnabledLangs([voice1]);
    });

    test('it does not show dropdown before click', () => {
      assertFalse(isPositionedOnPage(getDropdownItemForVoice(voice1)));
    });

    test('it shows dropdown items after button click', async () => {
      await openVoiceMenu();

      const dropdownItems: HTMLButtonElement = getDropdownItemForVoice(voice1);
      assertTrue(isPositionedOnPage(dropdownItems!));
      assertEquals(
          getDropdownItemForVoice(voice1)!.textContent!.trim(), voice1.name);
    });

    test('it shows language menu after button click', async () => {
      await openVoiceMenu();
      const button =
          voiceSelectionMenu.$.voiceSelectionMenu.get()
              .querySelector<HTMLButtonElement>('.language-menu-button');

      button!.click();
      await microtasksFinished();

      const languageMenuElement =
          voiceSelectionMenu.shadowRoot!.querySelector<LanguageMenuElement>(
              '#languageMenu');
      assertTrue(!!languageMenuElement);
      assertTrue(isPositionedOnPage(languageMenuElement));
    });

    test('when availableVoices updates', async () => {
      await setAvailableVoicesAndEnabledLangs(
          /*availableVoices=*/[voice1, voice2]);
      await openVoiceMenu();

      assertEquals(
          voice1.name, getDropdownItemForVoice(voice1).textContent!.trim());
      assertEquals(
          voice2.name, getDropdownItemForVoice(voice2).textContent!.trim());
      assertTrue(isPositionedOnPage(getDropdownItemForVoice(voice1)));
      assertTrue(isPositionedOnPage(getDropdownItemForVoice(voice2)));
    });
  });

  // <if expr="not is_chromeos">
  test('it renames non-Google voices per language', async () => {
    const googleVoice1 =
        createSpeechSynthesisVoice({name: 'Google Gandalf', lang: 'en-US'});
    const googleVoice2 =
        createSpeechSynthesisVoice({name: 'Google Gimli', lang: 'pt-BR'});
    const systemVoice1 =
        createSpeechSynthesisVoice({name: 'Legolas', lang: 'en-US'});
    const systemVoice2 =
        createSpeechSynthesisVoice({name: 'Frodo', lang: 'pt-BR'});
    const availableVoices = [
      googleVoice1,
      googleVoice2,
      systemVoice1,
      systemVoice2,
    ];
    await setAvailableVoicesAndEnabledLangs(
        availableVoices, ['en-US', 'pt-BR']);

    await openVoiceMenu();

    const menu = voiceSelectionMenu.$.voiceSelectionMenu.get();
    const groupTitles = menu.querySelectorAll<HTMLElement>('.lang-group-title');
    assertEquals(2, groupTitles.length);
    const voiceNames = menu.querySelectorAll<HTMLElement>('.voice-name');
    assertEquals(4, voiceNames.length);
    const englishVoice1 = groupTitles.item(0)!.nextElementSibling!;
    const englishVoice2 = englishVoice1.nextElementSibling!;
    const portugueseVoice1 = groupTitles.item(1)!.nextElementSibling!;
    const portugueseVoice2 = portugueseVoice1.nextElementSibling!;
    assertEquals(googleVoice1.name, englishVoice1.textContent!.trim());
    assertEquals(
        'System text-to-speech voice', englishVoice2.textContent!.trim());
    assertEquals(googleVoice2.name, portugueseVoice1.textContent!.trim());
    assertEquals(
        'System text-to-speech voice', portugueseVoice2.textContent!.trim());
  });
  // </if>

  suite('with multiple available voices', () => {
    let selectedVoice: SpeechSynthesisVoice;
    let previewVoice: SpeechSynthesisVoice;

    setup(() => {
      selectedVoice =
          createSpeechSynthesisVoice({name: 'Google selected', lang: 'en-US'});
      previewVoice =
          createSpeechSynthesisVoice({name: 'Google preview', lang: 'en-US'});
      voice1 =
          createSpeechSynthesisVoice({name: 'Google voice1', lang: 'en-US'});
      voice2 =
          createSpeechSynthesisVoice({name: 'Google voice2', lang: 'it-IT'});
      const availableVoices = [
        voice1,
        previewVoice,
        voice2,
        selectedVoice,
      ];
      return setAvailableVoicesAndEnabledLangs(availableVoices);
    });

    test('it shows a checkmark for the selected voice', async () => {
      voiceSelectionMenu.selectedVoice = selectedVoice;
      await microtasksFinished();

      const checkMarkVoice0 =
          getDropdownItemForVoice(voice1).querySelector<HTMLElement>(
              '#check-mark')!;
      const checkMarkSelectedVoice =
          getDropdownItemForVoice(selectedVoice)
              .querySelector<HTMLElement>('#check-mark')!;

      assertFalse(hasStyle(checkMarkSelectedVoice, 'visibility', 'hidden'));
      assertTrue(hasStyle(checkMarkVoice0, 'visibility', 'hidden'));
    });

    test('it groups voices by language', () => {
      const menu = voiceSelectionMenu.$.voiceSelectionMenu.get();
      const groupTitles =
          menu.querySelectorAll<HTMLElement>('.lang-group-title');
      assertEquals(2, groupTitles.length);

      const firstVoice = groupTitles.item(0)!.nextElementSibling!;
      const secondVoice = firstVoice.nextElementSibling!;
      const thirdVoice = secondVoice.nextElementSibling!;
      const italianVoice = groupTitles.item(1)!.nextElementSibling!;
      assertEquals(voice1.name, firstVoice.textContent!.trim());
      assertEquals(previewVoice.name, secondVoice.textContent!.trim());
      assertEquals(selectedVoice.name, thirdVoice.textContent!.trim());
      assertEquals(voice2.name, italianVoice.textContent!.trim());
    });

    test('it only shows enabled languages', async () => {
      await setAvailableVoicesAndEnabledLangs(
          voiceSelectionMenu.availableVoices, ['it-it']);

      const menu = voiceSelectionMenu.$.voiceSelectionMenu.get();
      const groupTitles =
          menu.querySelectorAll<HTMLElement>('.lang-group-title');
      assertEquals(1, groupTitles.length);

      const italianVoice = groupTitles.item(0)!.nextElementSibling!;
      assertEquals(voice2.name, italianVoice.textContent!.trim());
    });

    suite('with Natural voices also available', () => {
      setup(() => {
        voice1 = createSpeechSynthesisVoice(
            {name: 'Google US English 1 (Natural)', lang: 'en-US'});
        voice2 = createSpeechSynthesisVoice(
            {name: 'Google US English 2 (Natural)', lang: 'en-US'});

        const availableVoices = [
          previewVoice,
          voice1,
          voice2,
          selectedVoice,
        ];
        return setAvailableVoicesAndEnabledLangs(availableVoices);
      });

      test('it orders Natural voices first', () => {
        const usEnglishDropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu.$.voiceSelectionMenu.get().querySelectorAll(
                '.voice-name');

        assertEquals(4, usEnglishDropdownItems.length);
        assertEquals(
            'Google US English 1 (Natural)',
            usEnglishDropdownItems.item(0).textContent!.trim());
        assertEquals(
            'Google US English 2 (Natural)',
            usEnglishDropdownItems.item(1).textContent!.trim());
        assertEquals(
            previewVoice.name,
            usEnglishDropdownItems.item(2).textContent!.trim());
        assertEquals(
            selectedVoice.name,
            usEnglishDropdownItems.item(3).textContent!.trim());
      });
    });

    test('with display names for locales', async () => {
      voiceSelectionMenu.localeToDisplayName = {
        'en-us': 'English (United States)',
      };
      await microtasksFinished();

      const groupTitles =
          voiceSelectionMenu.$.voiceSelectionMenu.get()
              .querySelectorAll<HTMLElement>('.lang-group-title');

      assertEquals(
          'English (United States)', groupTitles.item(0)!.textContent!.trim());
      assertEquals('it-it', groupTitles.item(1)!.textContent!.trim());
    });

    test('languages are grouped when voices have same names', async () => {
      const availableVoices = [
        createSpeechSynthesisVoice({name: 'Google English', lang: 'en-US'}),
        createSpeechSynthesisVoice({name: 'Google English', lang: 'en-US'}),
        createSpeechSynthesisVoice({name: 'Google English', lang: 'en-UK'}),
      ];
      await setAvailableVoicesAndEnabledLangs(availableVoices);

      const menu = voiceSelectionMenu.$.voiceSelectionMenu.get();
      const groupTitles =
          menu.querySelectorAll<HTMLElement>('.lang-group-title');
      const voiceNames = menu.querySelectorAll<HTMLElement>('.voice-name');

      assertEquals(2, groupTitles.length);
      assertEquals('en-us', groupTitles.item(0)!.textContent!.trim());
      assertEquals('en-uk', groupTitles.item(1)!.textContent!.trim());
      assertEquals(3, voiceNames.length);
      assertEquals('Google English', voiceNames.item(0)!.textContent!.trim());
      assertEquals('Google English', voiceNames.item(1)!.textContent!.trim());
      assertEquals('Google English', voiceNames.item(2)!.textContent!.trim());
    });

    test('preview button click emits play preview event', async () => {
      let clickEmitted = false;
      document.addEventListener(
          ToolbarEvent.PLAY_PREVIEW, () => clickEmitted = true);
      await openVoiceMenu();
      const previewButton =
          getDropdownItemForVoice(voice1).querySelector<CrIconButtonElement>(
              '#preview-icon')!;

      previewButton!.click();
      await microtasksFinished();

      assertTrue(clickEmitted);
    });

    test('it shows preview-playing button when preview plays', async () => {
      await openVoiceMenu();
      voiceSelectionMenu.previewVoicePlaying = previewVoice;
      await microtasksFinished();

      const playIconVoice0 =
          getDropdownItemForVoice(voice1).querySelector<CrIconButtonElement>(
              '#preview-icon')!;
      const playIconOfPreviewVoice =
          getDropdownItemForVoice(previewVoice)
              .querySelector<CrIconButtonElement>('#preview-icon')!;

      // The play icon should flip to stop for the voice being previewed
      assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
      assertEquals(
          'read-anything-20:stop-circle', playIconOfPreviewVoice.ironIcon);
      assertStringContains(playIconOfPreviewVoice.title.toLowerCase(), 'stop');
      assertStringContains(
          playIconOfPreviewVoice.ariaLabel!.toLowerCase(), 'stop');
      // The play icon should remain unchanged for the other buttons
      assertTrue(isPositionedOnPage(playIconVoice0));
      assertEquals('read-anything-20:play-circle', playIconVoice0.ironIcon);
      assertStringContains(playIconVoice0.title.toLowerCase(), 'play');
      assertStringContains(
          playIconVoice0.ariaLabel!.toLowerCase(), 'preview voice for');
    });

    test('it shows play icon again when preview finishes', async () => {
      await openVoiceMenu();
      voiceSelectionMenu.previewVoicePlaying = previewVoice;
      await microtasksFinished();
      voiceSelectionMenu.previewVoicePlaying = undefined;
      await microtasksFinished();

      const playIconVoice0 =
          getDropdownItemForVoice(voice1).querySelector<CrIconButtonElement>(
              '#preview-icon')!;
      const playIconOfPreviewVoice =
          getDropdownItemForVoice(voice2).querySelector<CrIconButtonElement>(
              '#preview-icon')!;

      // All icons should be play icons because no preview is
      // playing
      assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
      assertTrue(isPositionedOnPage(playIconVoice0));
      assertEquals(
          'read-anything-20:play-circle', playIconOfPreviewVoice.ironIcon);
      assertEquals('read-anything-20:play-circle', playIconVoice0.ironIcon);
      assertStringContains(playIconOfPreviewVoice.title.toLowerCase(), 'play');
      assertStringContains(playIconVoice0.title.toLowerCase(), 'play');
      assertStringContains(
          playIconOfPreviewVoice.ariaLabel!.toLowerCase(), 'preview voice for');
      assertStringContains(
          playIconVoice0.ariaLabel!.toLowerCase(), 'preview voice for');
    });
  });

  // TODO(crbug.com/333596001): Add more keyboard tests here.
  suite('keyboard navigation', () => {
    function getDropdownItems() {
      return voiceSelectionMenu.$.voiceSelectionMenu.get()
          .querySelectorAll<HTMLButtonElement>(
              '.dropdown-voice-selection-button');
    }

    function getPreviewButton(dropdownItem: HTMLElement) {
      return dropdownItem.querySelector<HTMLElement>('#preview-icon')!;
    }

    setup(async () => {
      const voice1 =
          createSpeechSynthesisVoice({name: 'Google English', lang: 'en-US'});
      const voice2 =
          createSpeechSynthesisVoice({name: 'Google Spanish', lang: 'es-US'});
      const voice3 =
          createSpeechSynthesisVoice({name: 'Google French', lang: 'fr'});
      const availableVoices = [
        voice1,
        voice2,
        voice3,
      ];
      await setAvailableVoicesAndEnabledLangs(availableVoices);
      return openVoiceMenu();
    });

    test('tab stops are correct', () => {
      const items = getDropdownItems();
      const firstItem = items[0];

      // The first and last item should be reachable with tab.
      assertTrue(!!firstItem);
      assertEquals(0, firstItem.tabIndex);
      assertEquals(0, getPreviewButton(firstItem).tabIndex);
      assertEquals(0, items[items.length - 1]!.tabIndex);
      // The rest of the items should not be.
      for (let i = 1; i < items.length - 1; i++) {
        const item = items[i];
        assertTrue(!!item);
        assertEquals(-1, item.tabIndex);
        assertEquals(-1, getPreviewButton(item).tabIndex);
      }
    });

    test('tab closes menu only on language button', async () => {
      const items = getDropdownItems();

      // None of these should close the menu.
      for (let i = 0; i < items.length - 1; i++) {
        const item = items[i];
        assertTrue(!!item);
        item.focus();
        keyDownOn(item, 0, undefined, 'Tab');
        await microtasksFinished();
        assertTrue(voiceSelectionMenu.$.voiceSelectionMenu.get().open);
      }

      // Tab on the last item should close the menu.
      const item = items[items.length - 1];
      assertTrue(!!item);
      item.focus();
      keyDownOn(item, 0, undefined, 'Tab');
      await microtasksFinished();
      assertFalse(voiceSelectionMenu.$.voiceSelectionMenu.get().open);
    });
  });

  suite('with installing voices', () => {
    function setVoiceStatus(
        status: VoiceClientSideStatusCode, lang: string): Promise<void> {
      VoiceNotificationManager.getInstance().onVoiceStatusChange(
          lang, status, voiceSelectionMenu.availableVoices, true);
      return microtasksFinished();
    }

    function setOfflineError(lang: string): Promise<void> {
      VoiceNotificationManager.getInstance().onVoiceStatusChange(
          lang, VoiceClientSideStatusCode.ERROR_INSTALLING,
          voiceSelectionMenu.availableVoices, false);
      return microtasksFinished();
    }

    function getDownloadMessages(): HTMLElement[] {
      return Array.from(
          voiceSelectionMenu.$.voiceSelectionMenu.get()
              .querySelectorAll<HTMLElement>('.download-message'));
    }

    function getErrorMessages(): HTMLElement[] {
      return Array.from(voiceSelectionMenu.$.voiceSelectionMenu.get()
                            .querySelectorAll<HTMLElement>('.error-message'));
    }

    setup(() => {
      VoiceNotificationManager.getInstance().clear();
      return microtasksFinished();
    });

    test('no messages by default', async () => {
      await openVoiceMenu();

      assertEquals(0, getDownloadMessages().length);
      assertEquals(0, getErrorMessages().length);
    });

    suite('with one language', () => {
      const lang = 'fr';

      test('shows downloading message while installing', async () => {
        await openVoiceMenu();

        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang);

        const msgs = getDownloadMessages();
        assertEquals(1, msgs.length);
        assertEquals(0, getErrorMessages().length);
        assertStringContains(
            msgs[0]!.textContent!.trim(), 'Downloading Français voices');
      });

      test('hides downloading message when done', async () => {
        await openVoiceMenu();
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang);

        await setVoiceStatus(VoiceClientSideStatusCode.AVAILABLE, lang);

        assertEquals(0, getDownloadMessages().length);
      });

      test('shows error message when fail', async () => {
        await openVoiceMenu();

        await setOfflineError(lang);

        const msgs = getErrorMessages();
        assertEquals(0, getDownloadMessages().length);
        assertEquals(1, msgs.length);
        assertStringContains(msgs[0]!.textContent!, 'Connect to the internet');
      });

      test('shows downloading messages on open', async () => {
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang);

        await openVoiceMenu();

        assertEquals(1, getDownloadMessages().length);
        assertEquals(0, getErrorMessages().length);
      });

      test('does not show error messages on open', async () => {
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang);
        await setOfflineError(lang);

        await openVoiceMenu();

        assertEquals(0, getDownloadMessages().length);
        assertEquals(0, getErrorMessages().length);
      });
    });

    suite('with multiple languages', () => {
      const lang1 = 'en';
      const lang2 = 'ja';
      const lang3 = 'es-es';
      const lang4 = 'hi-HI';

      async function downloadLangs(): Promise<void> {
        await openVoiceMenu();
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang1);
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang2);
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang3);
        return setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang4);
      }

      test('shows downloading messages while installing', async () => {
        await downloadLangs();

        const msgs = getDownloadMessages();
        assertEquals(4, msgs.length);
        assertStringContains(
            msgs[0]!.textContent!,
            'Downloading English (United States) voices');
        assertStringContains(
            msgs[1]!.textContent!, 'Downloading 日本語 voices');
        assertStringContains(
            msgs[2]!.textContent!, 'Downloading Español (España) voices');
        assertStringContains(msgs[3]!.textContent!, 'Downloading हिन्दी voices');
      });

      test('hides downloading messages when done', async () => {
        await downloadLangs();

        await setVoiceStatus(VoiceClientSideStatusCode.AVAILABLE, lang1);
        assertEquals(3, getDownloadMessages().length);

        await setVoiceStatus(VoiceClientSideStatusCode.AVAILABLE, lang2);
        assertEquals(2, getDownloadMessages().length);

        await setVoiceStatus(VoiceClientSideStatusCode.AVAILABLE, lang3);
        assertEquals(1, getDownloadMessages().length);

        await setVoiceStatus(VoiceClientSideStatusCode.AVAILABLE, lang4);
        assertEquals(0, getDownloadMessages().length);
      });

      test('shows error message when fail', async () => {
        await openVoiceMenu();

        await setOfflineError(lang1);
        await setOfflineError(lang2);
        await setOfflineError(lang3);
        await setOfflineError(lang4);

        const msgs = getErrorMessages();
        assertEquals(0, getDownloadMessages().length);
        assertEquals(4, msgs.length);
        assertStringContains(
            msgs[0]!.textContent!,
            'There are no English (United States) voices');
        assertStringContains(
            msgs[1]!.textContent!, 'There are no 日本語 voices');
        assertStringContains(
            msgs[2]!.textContent!, 'There are no Español (España) voices');
        assertStringContains(
            msgs[3]!.textContent!, 'There are no हिन्दी voices');
      });

      test('shows only downloading messages on open', async () => {
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang1);
        await setVoiceStatus(
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, lang2);
        await setOfflineError(lang3);
        await setOfflineError(lang4);

        await openVoiceMenu();

        assertEquals(2, getDownloadMessages().length);
        assertEquals(0, getErrorMessages().length);
      });
    });
  });
});
