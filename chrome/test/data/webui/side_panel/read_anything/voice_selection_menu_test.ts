// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {PLAY_PREVIEW_EVENT, VoiceClientSideStatusCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceSelectionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {stubAnimationFrame} from './common.js';

function stringToHtmlTestId(s: string): string {
  return s.replace(/\s/g, '-').replace(/[()]/g, '');
}

suite('VoiceSelectionMenu', () => {
  let voiceSelectionMenu: VoiceSelectionMenuElement|null;
  let availableVoices: SpeechSynthesisVoice[];
  let myClickEvent: MouseEvent;

  const voiceSelectionButtonSelector: string =
      '.dropdown-voice-selection-button:not(.language-menu-button)';

  // If no param for enabledLangs is provided, it auto populates it with the
  // langs of the voices
  const setAvailableVoices = (enabledLangs: string[]|undefined = undefined) => {
    // Bypass Typescript compiler to allow us to set a private readonly
    // property
    // @ts-ignore
    voiceSelectionMenu.availableVoices = availableVoices;
    if (enabledLangs === undefined) {
      // @ts-ignore
      voiceSelectionMenu.enabledLanguagesInPref =
          [...new Set(availableVoices.map(({lang}) => lang))];
    } else {
      // @ts-ignore
      voiceSelectionMenu.enabledLanguagesInPref = enabledLangs;
    }
    flush();
  };

  const getDropdownItemForVoice = (voice: SpeechSynthesisVoice) => {
    return voiceSelectionMenu!.$.voiceSelectionMenu.get()
        .querySelector<HTMLButtonElement>(`[data-test-id="${
            stringToHtmlTestId(voice.name)}"].dropdown-voice-selection-button`)!
        ;
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    voiceSelectionMenu = document.createElement('voice-selection-menu');
    document.body.appendChild(voiceSelectionMenu);

    // Proxy button as click target to open the menu with
    const dots: HTMLElement = document.createElement('button');
    const newContent = document.createTextNode('...');
    dots.appendChild(newContent);
    document.body.appendChild(dots);
    myClickEvent = {target: dots} as unknown as MouseEvent;

    // @ts-ignore
    voiceSelectionMenu.voicePackInstallStatus = {};

    flush();
  });

  suite('with one voice', () => {
    setup(() => {
      availableVoices =
          [{name: 'test voice 1', lang: 'lang'} as SpeechSynthesisVoice];
      setAvailableVoices();
    });

    test('it does not show dropdown before click', () => {
      assertFalse(
          isPositionedOnPage(getDropdownItemForVoice(availableVoices[0]!)));
    });

    test('it shows dropdown items after button click', () => {
      stubAnimationFrame();
      voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);

      flush();

      assertTrue(
          isPositionedOnPage(getDropdownItemForVoice(availableVoices[0]!)!));
      assertEquals(
          getDropdownItemForVoice(availableVoices[0]!)!.textContent!.trim(),
          availableVoices[0]!.name);
    });

    test('it shows language menu after button click', () => {
      stubAnimationFrame();
      const button =
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelector<HTMLButtonElement>('.language-menu-button');
      button!.click();
      const languageMenuElement: LanguageMenuElement =
          voiceSelectionMenu!.$.languageMenu.get();
      assertTrue(isPositionedOnPage(languageMenuElement.$.languageMenu));
      flush();
    });

    suite('when availableVoices updates', () => {
      setup(() => {
        availableVoices = [
          {name: 'test voice 1', lang: 'lang'} as SpeechSynthesisVoice,
          {name: 'test voice 2', lang: 'lang'} as SpeechSynthesisVoice,
        ];
        setAvailableVoices();
      });

      test('it updates and displays the new voices', () => {
        stubAnimationFrame();
        voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);
        flush();

        const dropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelectorAll<HTMLButtonElement>(
                    voiceSelectionButtonSelector);

        assertEquals(
            getDropdownItemForVoice(availableVoices[0]!).textContent!.trim(),
            availableVoices[0]!.name);
        assertEquals(
            getDropdownItemForVoice(availableVoices[1]!).textContent!.trim(),
            availableVoices[1]!.name);
        assertEquals(dropdownItems.length, 2);
        assertTrue(
            isPositionedOnPage(getDropdownItemForVoice(availableVoices[0]!)));
        assertTrue(
            isPositionedOnPage(getDropdownItemForVoice(availableVoices[1]!)));
      });
    });
  });

  suite('with multiple available voices', () => {
    let selectedVoice: SpeechSynthesisVoice;
    let previewVoice: SpeechSynthesisVoice;

    setup(() => {
      // We need an additional call to voiceSelectionMenu.get() in these
      // tests to ensure the menu has been rendered.
      voiceSelectionMenu!.$.voiceSelectionMenu.get();
      selectedVoice = {name: 'test voice 3', lang: 'en-US'} as
          SpeechSynthesisVoice;
      previewVoice = {name: 'test voice 1', lang: 'en-US'} as
          SpeechSynthesisVoice;

      availableVoices = [
        {name: 'test voice 0', lang: 'en-US'} as SpeechSynthesisVoice,
        previewVoice,
        {name: 'test voice 2', lang: 'it-IT'} as SpeechSynthesisVoice,
        selectedVoice,
      ];
      setAvailableVoices();
    });

    test('it shows a checkmark for the selected voice', () => {
      // Bypass Typescript compiler to allow us to set a private readonly
      // property
      // @ts-ignore
      voiceSelectionMenu.selectedVoice = selectedVoice;
      flush();

      const checkMarkVoice0 = getDropdownItemForVoice(availableVoices[0]!)
                                  .querySelector<HTMLElement>('#check-mark')!;
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
      assertEquals(groupTitles.length, 2);

      const firstVoice = groupTitles.item(0)!.nextElementSibling!;
      const secondVoice = firstVoice.nextElementSibling!;
      const thirdVoice = secondVoice.nextElementSibling!;
      const italianVoice = groupTitles.item(1)!.nextElementSibling!;
      assertEquals(firstVoice.textContent!.trim(), 'test voice 0');
      assertEquals(secondVoice.textContent!.trim(), 'test voice 1');
      assertEquals(thirdVoice.textContent!.trim(), 'test voice 3');
      assertEquals(italianVoice.textContent!.trim(), 'test voice 2');
    });

    suite('with some disabled languages', () => {
      test('it only shows enabled languages', () => {
        setAvailableVoices(['it-it']);

        const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
        const groupTitles =
            menu.querySelectorAll<HTMLElement>('.lang-group-title');
        assertEquals(groupTitles.length, 1);

        const italianVoice = groupTitles.item(0)!.nextElementSibling!;
        assertEquals(italianVoice.textContent!.trim(), 'test voice 2');
      });
    });

    suite('with Natural voices also available', () => {
      setup(() => {
        availableVoices = [
          previewVoice,
          {name: 'Google US English 1 (Natural)', lang: 'en-US'} as
              SpeechSynthesisVoice,
          {name: 'Google US English 2 (Natural)', lang: 'en-US'} as
              SpeechSynthesisVoice,
          selectedVoice,
        ];
        setAvailableVoices();
      });

      test('it orders Natural voices first', () => {
        const usEnglishDropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu!.$.voiceSelectionMenu.get().querySelectorAll(
                '.voice-name');

        assertEquals(usEnglishDropdownItems.length, 4);
        assertEquals(
            usEnglishDropdownItems.item(0).textContent!.trim(),
            'Google US English 1 (Natural)');
        assertEquals(
            usEnglishDropdownItems.item(1).textContent!.trim(),
            'Google US English 2 (Natural)');
        assertEquals(
            usEnglishDropdownItems.item(2).textContent!.trim(), 'test voice 1');
        assertEquals(
            usEnglishDropdownItems.item(3).textContent!.trim(), 'test voice 3');
      });
    });

    suite('with display names for locales', () => {
      let groupTitles: NodeListOf<HTMLElement>;

      setup(() => {
        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        voiceSelectionMenu.localeToDisplayName = {
          'en-US': 'English (United States)',
        };
        flush();
        groupTitles = voiceSelectionMenu!.$.voiceSelectionMenu.get()
                          .querySelectorAll<HTMLElement>('.lang-group-title');
      });

      test('it displays the display name', () => {
        assertEquals(
            groupTitles.item(0)!.textContent!.trim(),
            'English (United States)');
      });

      test('it defaults to the locale when there is no display name', () => {
        assertEquals(groupTitles.item(1)!.textContent!.trim(), 'it-IT');
      });
    });

    suite('when voices have duplicate names', () => {
      setup(() => {
        availableVoices = [
          {name: 'English', lang: 'en-US'} as SpeechSynthesisVoice,
          {name: 'English', lang: 'en-US'} as SpeechSynthesisVoice,
          {name: 'English', lang: 'en-UK'} as SpeechSynthesisVoice,
        ];
        setAvailableVoices();
      });

      test('it groups the duplicate languages correctly', () => {
        const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
        const groupTitles =
            menu.querySelectorAll<HTMLElement>('.lang-group-title');
        const voiceNames = menu.querySelectorAll<HTMLElement>('.voice-name');

        assertEquals(groupTitles.length, 2);
        assertEquals(groupTitles.item(0)!.textContent!.trim(), 'en-US');
        assertEquals(groupTitles.item(1)!.textContent!.trim(), 'en-UK');
        assertEquals(voiceNames.length, 3);
        assertEquals(voiceNames.item(0)!.textContent!.trim(), 'English');
        assertEquals(voiceNames.item(1)!.textContent!.trim(), 'English');
        assertEquals(voiceNames.item(2)!.textContent!.trim(), 'English');
      });
    });

    suite('when preview button is clicked', () => {
      let clickEmitted: boolean;

      setup(() => {
        clickEmitted = false;
        document.addEventListener(
            PLAY_PREVIEW_EVENT, () => clickEmitted = true);

        // Display dropdown menu
        voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);
        flush();
      });

      test('it emits play preview event', () => {
        const previewButton =
            getDropdownItemForVoice(availableVoices[0]!)
                .querySelector<CrIconButtonElement>('#preview-icon')!;

        previewButton!.click();
        flush();
        assertTrue(clickEmitted);
      });
    });

    suite('when preview starts playing', () => {
      setup(() => {
        // Display dropdown menu
        voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);

        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        voiceSelectionMenu.previewVoicePlaying = previewVoice;
        flush();
      });

      test('it shows preview-playing button when preview plays', () => {
        stubAnimationFrame();
        const playIconVoice0 =
            getDropdownItemForVoice(availableVoices[0]!)
                .querySelector<CrIconButtonElement>('#preview-icon')!;
        const playIconOfPreviewVoice =
            getDropdownItemForVoice(previewVoice)
                .querySelector<CrIconButtonElement>('#preview-icon')!;

        // The play icon should flip to stop for the voice being previewed
        assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
        assertEquals(
            playIconOfPreviewVoice.ironIcon, 'read-anything-20:stop-circle');
        assertStringContains(
            playIconOfPreviewVoice.title.toLowerCase(), 'stop');
        assertStringContains(
            playIconOfPreviewVoice.ariaLabel!.toLowerCase(), 'stop');
        // The play icon should remain unchanged for the other buttons
        assertTrue(isPositionedOnPage(playIconVoice0));
        assertEquals(playIconVoice0.ironIcon, 'read-anything-20:play-circle');
        assertStringContains(playIconVoice0.title.toLowerCase(), 'play');
        assertStringContains(
            playIconVoice0.ariaLabel!.toLowerCase(), 'preview voice for');
      });

      suite('when preview finishes playing', () => {
        setup(() => {
          // Bypass Typescript compiler to allow us to set a private readonly
          // property
          // @ts-ignore
          voiceSelectionMenu.previewVoicePlaying = null;
          flush();
        });

        test('it flips the preview button back to play icon', () => {
          stubAnimationFrame();
          const playIconVoice0 =
              getDropdownItemForVoice(availableVoices[0]!)
                  .querySelector<CrIconButtonElement>('#preview-icon')!;
          const playIconOfPreviewVoice =
              getDropdownItemForVoice(availableVoices[1]!)
                  .querySelector<CrIconButtonElement>('#preview-icon')!;

          // All icons should be play icons because no preview is
          // playing
          assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
          assertTrue(isPositionedOnPage(playIconVoice0));
          assertEquals(
              playIconOfPreviewVoice.ironIcon, 'read-anything-20:play-circle');
          assertEquals(playIconVoice0.ironIcon, 'read-anything-20:play-circle');
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
  });

  suite('with installing voices', () => {
    function setVoiceStatus(lang: string, status: VoiceClientSideStatusCode) {
      // @ts-ignore
      voiceSelectionMenu.voicePackInstallStatus = {
        // @ts-ignore
        ...voiceSelectionMenu.voicePackInstallStatus,
        [lang]: status,
      };
      flush();
    }

    function startDownload(lang: string) {
      setVoiceStatus(lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
    }

    function finishDownload(lang: string) {
      setVoiceStatus(lang, VoiceClientSideStatusCode.AVAILABLE);
    }

    function getDownloadMessages(): HTMLElement[] {
      return Array.from(
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelectorAll<HTMLElement>('.download-message'));
    }

    setup(() => {
      voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);
      flush();
    });

    test('no downloading messages by default', () => {
      assertEquals(getDownloadMessages().length, 0);
    });

    test('no downloading messages with invalid language', () => {
      startDownload('simlish');
      assertEquals(getDownloadMessages().length, 0);
    });

    suite('with one language', () => {
      const lang = 'fr';

      setup(() => {
        startDownload(lang);
      });

      test('shows downloading message while installing', () => {
        const msgs = getDownloadMessages();

        assertEquals(msgs.length, 1);
        assertStringContains(
            msgs[0]!.textContent!.trim(), 'Downloading Français voices');
      });

      test('hides downloading message when done', () => {
        finishDownload(lang);
        assertEquals(getDownloadMessages().length, 0);
      });
    });

    suite('with multiple languages', () => {
      const lang1 = 'en';
      const lang2 = 'ja';
      const lang3 = 'es-es';
      const lang4 = 'hi-HI';

      setup(() => {
        startDownload(lang1);
        startDownload(lang2);
        startDownload(lang3);
        startDownload(lang4);
      });

      test('shows downloading messages while installing', () => {
        const msgs = getDownloadMessages();

        assertEquals(msgs.length, 4);
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

      test('hides downloading messages when done', () => {
        finishDownload(lang1);
        assertEquals(getDownloadMessages().length, 3);

        finishDownload(lang2);
        assertEquals(getDownloadMessages().length, 2);

        finishDownload(lang3);
        assertEquals(getDownloadMessages().length, 1);

        finishDownload(lang4);
        assertEquals(getDownloadMessages().length, 0);
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
