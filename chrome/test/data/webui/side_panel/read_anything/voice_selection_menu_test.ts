// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {LanguageMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent, VoiceClientSideStatusCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceSelectionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, stubAnimationFrame} from './common.js';

function stringToHtmlTestId(s: string): string {
  return s.replace(/\s/g, '-').replace(/[()]/g, '');
}

suite('VoiceSelectionMenu', () => {
  let voiceSelectionMenu: VoiceSelectionMenuElement;
  let dots: HTMLElement;
  let voice1 = createSpeechSynthesisVoice({name: 'test voice 1', lang: 'lang'});
  let voice2 = createSpeechSynthesisVoice({name: 'test voice 2', lang: 'lang'});

  const voiceSelectionButtonSelector: string =
      '.dropdown-voice-selection-button:not(.language-menu-button)';

  // If no param for enabledLangs is provided, it auto populates it with the
  // langs of the voices
  function setAvailableVoicesAndEnabledLangs(
      availableVoices: SpeechSynthesisVoice[], enabledLangs?: string[]) {
    voiceSelectionMenu.availableVoices = availableVoices;
    if (enabledLangs === undefined) {
      voiceSelectionMenu.enabledLangs =
          [...new Set(availableVoices.map(({lang}) => lang))];
    } else {
      voiceSelectionMenu.enabledLangs = enabledLangs;
    }
  }

  function getDropdownItemForVoice(voice: SpeechSynthesisVoice) {
    return voiceSelectionMenu.$.voiceSelectionMenu.get()
        .querySelector<HTMLButtonElement>(`[data-test-id="${
            stringToHtmlTestId(voice.name)}"].dropdown-voice-selection-button`)!
        ;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    voiceSelectionMenu = document.createElement('voice-selection-menu');
    document.body.appendChild(voiceSelectionMenu);

    // Proxy button as click target to open the menu with
    dots = document.createElement('button');
    const newContent = document.createTextNode('...');
    dots.appendChild(newContent);
    document.body.appendChild(dots);

    voiceSelectionMenu.voicePackInstallStatus = {};
    return microtasksFinished();
  });

  suite('with one voice', () => {
    setup(() => {
      setAvailableVoicesAndEnabledLangs([voice1]);

      return microtasksFinished();
    });

    test('it does not show dropdown before click', () => {
      assertFalse(isPositionedOnPage(getDropdownItemForVoice(voice1)));
    });

    test('it shows dropdown items after button click', async () => {
      stubAnimationFrame();
      voiceSelectionMenu!.onVoiceSelectionMenuClick(dots);
      await microtasksFinished();

      const dropdownItems: HTMLButtonElement = getDropdownItemForVoice(voice1);
      assertTrue(isPositionedOnPage(dropdownItems!));
      assertEquals(
          getDropdownItemForVoice(voice1)!.textContent!.trim(), voice1.name);
    });

    test('it shows language menu after button click', async () => {
      stubAnimationFrame();
      const button =
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelector<HTMLButtonElement>('.language-menu-button');
      button!.click();
      await microtasksFinished();

      const languageMenuElement =
          voiceSelectionMenu!.shadowRoot!.querySelector<LanguageMenuElement>(
              '#languageMenu');
      assertTrue(!!languageMenuElement);
      assertTrue(isPositionedOnPage(languageMenuElement));
    });

    test('when availableVoices updates', async () => {
      setAvailableVoicesAndEnabledLangs(/*availableVoices=*/[voice1, voice2]);
      stubAnimationFrame();
      voiceSelectionMenu!.onVoiceSelectionMenuClick(dots);
      await microtasksFinished();

      const dropdownItems: NodeListOf<HTMLElement> =
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelectorAll<HTMLButtonElement>(
                  voiceSelectionButtonSelector);

      assertEquals(
          voice1.name, getDropdownItemForVoice(voice1).textContent!.trim());
      assertEquals(
          voice2.name, getDropdownItemForVoice(voice2).textContent!.trim());
      assertEquals(2, dropdownItems.length);
      assertTrue(isPositionedOnPage(getDropdownItemForVoice(voice1)));
      assertTrue(isPositionedOnPage(getDropdownItemForVoice(voice2)));
    });
  });

  suite('with multiple available voices', () => {
    let selectedVoice: SpeechSynthesisVoice;
    let previewVoice: SpeechSynthesisVoice;

    setup(() => {
      // We need an additional call to voiceSelectionMenu.get() in these
      // tests to ensure the menu has been rendered.
      voiceSelectionMenu!.$.voiceSelectionMenu.get();
      selectedVoice =
          createSpeechSynthesisVoice({name: 'selected', lang: 'en-US'});
      previewVoice =
          createSpeechSynthesisVoice({name: 'preview', lang: 'en-US'});
      voice1 = createSpeechSynthesisVoice({name: 'voice1', lang: 'en-US'});
      voice2 = createSpeechSynthesisVoice({name: 'voice2', lang: 'it-IT'});
      const availableVoices = [
        voice1,
        previewVoice,
        voice2,
        selectedVoice,
      ];
      setAvailableVoicesAndEnabledLangs(availableVoices);
      return microtasksFinished();
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

      assertFalse(isHiddenWithCss(checkMarkSelectedVoice));
      assertTrue(isHiddenWithCss(checkMarkVoice0));
    });

    test('it groups voices by language', () => {
      const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
      const groupTitles =
          menu.querySelectorAll<HTMLElement>('.lang-group-title');
      assertEquals(2, groupTitles.length);

      const firstVoice = groupTitles.item(0)!.nextElementSibling!;
      const secondVoice = firstVoice.nextElementSibling!;
      const thirdVoice = secondVoice.nextElementSibling!;
      const italianVoice = groupTitles.item(1)!.nextElementSibling!;
      assertEquals('voice1', firstVoice.textContent!.trim());
      assertEquals('preview', secondVoice.textContent!.trim());
      assertEquals('selected', thirdVoice.textContent!.trim());
      assertEquals('voice2', italianVoice.textContent!.trim());
    });

    test(
        'it only shows enabled languages with some disabled languages',
        async () => {
          setAvailableVoicesAndEnabledLangs(
              voiceSelectionMenu.availableVoices, ['it-it']);
          await microtasksFinished();

          const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
          const groupTitles =
              menu.querySelectorAll<HTMLElement>('.lang-group-title');
          assertEquals(1, groupTitles.length);

          const italianVoice = groupTitles.item(0)!.nextElementSibling!;
          assertEquals('voice2', italianVoice.textContent!.trim());
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
        setAvailableVoicesAndEnabledLangs(availableVoices);

        return microtasksFinished();
      });

      test('it orders Natural voices first', () => {
        const usEnglishDropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu!.$.voiceSelectionMenu.get().querySelectorAll(
                '.voice-name');

        assertEquals(4, usEnglishDropdownItems.length);
        assertEquals(
            'Google US English 1 (Natural)',
            usEnglishDropdownItems.item(0).textContent!.trim());
        assertEquals(
            'Google US English 2 (Natural)',
            usEnglishDropdownItems.item(1).textContent!.trim());
        assertEquals(
            'preview', usEnglishDropdownItems.item(2).textContent!.trim());
        assertEquals(
            'selected', usEnglishDropdownItems.item(3).textContent!.trim());
      });
    });

    test('with display names for locales', async () => {
      voiceSelectionMenu.localeToDisplayName = {
        'en-us': 'English (United States)',
      };
      await microtasksFinished();

      const groupTitles =
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelectorAll<HTMLElement>('.lang-group-title');

      assertEquals(
          'English (United States)', groupTitles.item(0)!.textContent!.trim());
      assertEquals('it-it', groupTitles.item(1)!.textContent!.trim());
    });

    test(
        'when voices have duplicate names languages are grouped correctly',
        async () => {
          const availableVoices = [
            createSpeechSynthesisVoice({name: 'English', lang: 'en-US'}),
            createSpeechSynthesisVoice({name: 'English', lang: 'en-US'}),
            createSpeechSynthesisVoice({name: 'English', lang: 'en-UK'}),
          ];
          setAvailableVoicesAndEnabledLangs(availableVoices);
          await microtasksFinished();

          const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
          const groupTitles =
              menu.querySelectorAll<HTMLElement>('.lang-group-title');
          const voiceNames = menu.querySelectorAll<HTMLElement>('.voice-name');

          assertEquals(2, groupTitles.length);
          assertEquals('en-us', groupTitles.item(0)!.textContent!.trim());
          assertEquals('en-uk', groupTitles.item(1)!.textContent!.trim());
          assertEquals(3, voiceNames.length);
          assertEquals('English', voiceNames.item(0)!.textContent!.trim());
          assertEquals('English', voiceNames.item(1)!.textContent!.trim());
          assertEquals('English', voiceNames.item(2)!.textContent!.trim());
        });

    test(
        'when preview button is clicked it emits play preview event',
        async () => {
          let clickEmitted = false;
          document.addEventListener(
              ToolbarEvent.PLAY_PREVIEW, () => clickEmitted = true);
          // Display dropdown menu
          voiceSelectionMenu!.onVoiceSelectionMenuClick(dots);
          const previewButton =
              getDropdownItemForVoice(voice1)
                  .querySelector<CrIconButtonElement>('#preview-icon')!;
          previewButton!.click();
          await microtasksFinished();

          assertTrue(clickEmitted);
        });

    suite('when preview starts playing', () => {
      setup(() => {
        // Display dropdown menu
        voiceSelectionMenu.onVoiceSelectionMenuClick(dots);

        voiceSelectionMenu.previewVoicePlaying = previewVoice;
        return microtasksFinished();
      });

      test('it shows preview-playing button when preview plays', () => {
        stubAnimationFrame();
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
        assertStringContains(
            playIconOfPreviewVoice.title.toLowerCase(), 'stop');
        assertStringContains(
            playIconOfPreviewVoice.ariaLabel!.toLowerCase(), 'stop');
        // The play icon should remain unchanged for the other buttons
        assertTrue(isPositionedOnPage(playIconVoice0));
        assertEquals('read-anything-20:play-circle', playIconVoice0.ironIcon);
        assertStringContains(playIconVoice0.title.toLowerCase(), 'play');
        assertStringContains(
            playIconVoice0.ariaLabel!.toLowerCase(), 'preview voice for');
      });

      test(
          'when preview finishes playing it flips the button back to play icon',
          async () => {
            voiceSelectionMenu.previewVoicePlaying = undefined;
            await microtasksFinished();

            stubAnimationFrame();
            const playIconVoice0 =
                getDropdownItemForVoice(voice1)
                    .querySelector<CrIconButtonElement>('#preview-icon')!;
            const playIconOfPreviewVoice =
                getDropdownItemForVoice(voice2)
                    .querySelector<CrIconButtonElement>('#preview-icon')!;

            // All icons should be play icons because no preview is
            // playing
            assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
            assertTrue(isPositionedOnPage(playIconVoice0));
            assertEquals(
                'read-anything-20:play-circle',
                playIconOfPreviewVoice.ironIcon);
            assertEquals(
                'read-anything-20:play-circle', playIconVoice0.ironIcon);
            assertStringContains(
                playIconOfPreviewVoice.title.toLowerCase(), 'play');
            assertStringContains(playIconVoice0.title.toLowerCase(), 'play');
            assertStringContains(
                playIconOfPreviewVoice.ariaLabel!.toLowerCase(),
                'preview voice for');
            assertStringContains(
                playIconVoice0.ariaLabel!.toLowerCase(), 'preview voice for');
          });
    });
  });

  suite('with installing voices', () => {
    function setVoiceStatus(lang: string, status: VoiceClientSideStatusCode) {
      voiceSelectionMenu.voicePackInstallStatus = {
        ...voiceSelectionMenu.voicePackInstallStatus,
        [lang]: status,
      };
    }


    function getDownloadMessages(): HTMLElement[] {
      return Array.from(
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelectorAll<HTMLElement>('.download-message'));
    }

    setup(() => {
      voiceSelectionMenu!.onVoiceSelectionMenuClick(dots);
    });

    test('no downloading messages by default', () => {
      assertEquals(0, getDownloadMessages().length);
    });

    test('no downloading messages with invalid language', async () => {
      setVoiceStatus('simlish', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
      await microtasksFinished();

      assertEquals(0, getDownloadMessages().length);
    });

    suite('with one language', () => {
      const lang = 'fr';

      setup(() => {
        setVoiceStatus(lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        return microtasksFinished();
      });

      test('shows downloading message while installing', () => {
        const msgs = getDownloadMessages();

        assertEquals(1, msgs.length);
        assertStringContains(
            msgs[0]!.textContent!.trim(), 'Downloading Français voices');
      });

      test('hides downloading message when done', async () => {
        setVoiceStatus(lang, VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();

        assertEquals(0, getDownloadMessages().length);
      });
    });

    suite('with multiple languages', () => {
      const lang1 = 'en';
      const lang2 = 'ja';
      const lang3 = 'es-es';
      const lang4 = 'hi-HI';

      setup(() => {
        setVoiceStatus(lang1, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        setVoiceStatus(lang2, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        setVoiceStatus(lang3, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        setVoiceStatus(lang4, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
        return microtasksFinished();
      });

      test('shows downloading messages while installing', () => {
        const msgs = getDownloadMessages();

        assertEquals(4, msgs.length);
        assertStringContains(
            msgs[0]!.textContent!.trim(),
            'Downloading English (United States) voices');
        assertStringContains(
            msgs[1]!.textContent!.trim(), 'Downloading 日本語 voices');
        assertStringContains(
            msgs[2]!.textContent!.trim(),
            'Downloading Español (España) voices');
        assertStringContains(
            msgs[3]!.textContent!.trim(),
            'Downloading हिन्दी voices');
      });

      test('hides downloading messages when done', async () => {
        setVoiceStatus(lang1, VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();
        assertEquals(3, getDownloadMessages().length);

        setVoiceStatus(lang2, VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();
        assertEquals(2, getDownloadMessages().length);

        setVoiceStatus(lang3, VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();
        assertEquals(1, getDownloadMessages().length);

        setVoiceStatus(lang4, VoiceClientSideStatusCode.AVAILABLE);
        await microtasksFinished();
        assertEquals(0, getDownloadMessages().length);
      });
    });
  });
});

function isHiddenWithCss(element: HTMLElement): boolean {
  return window.getComputedStyle(element).visibility === 'hidden';
}

function isPositionedOnPage(element: HTMLElement) {
  return !!element &&
      !!(element.offsetWidth || element.offsetHeight ||
         element.getClientRects().length);
}
