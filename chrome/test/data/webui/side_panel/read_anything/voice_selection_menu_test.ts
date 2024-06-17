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

import {createSpeechSynthesisVoice, stubAnimationFrame} from './common.js';

function stringToHtmlTestId(s: string): string {
  return s.replace(/\s/g, '-').replace(/[()]/g, '');
}

suite('VoiceSelectionMenu', () => {
  let voiceSelectionMenu: VoiceSelectionMenuElement;
  let availableVoices: SpeechSynthesisVoice[];
  let myClickEvent: MouseEvent;

  const voiceSelectionButtonSelector: string =
      '.dropdown-voice-selection-button:not(.language-menu-button)';

  // If no param for enabledLangs is provided, it auto populates it with the
  // langs of the voices
  function setAvailableVoices(enabledLangs?: string[]) {
    voiceSelectionMenu.availableVoices = availableVoices;
    if (enabledLangs === undefined) {
      voiceSelectionMenu.enabledLangs =
          [...new Set(availableVoices.map(({lang}) => lang))];
    } else {
      voiceSelectionMenu.enabledLangs = enabledLangs;
    }
    flush();
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
    const dots: HTMLElement = document.createElement('button');
    const newContent = document.createTextNode('...');
    dots.appendChild(newContent);
    document.body.appendChild(dots);
    myClickEvent = {target: dots} as unknown as MouseEvent;

    voiceSelectionMenu.voicePackInstallStatus = {};

    flush();
  });

  suite('with one voice', () => {
    setup(() => {
      availableVoices =
          [createSpeechSynthesisVoice({name: 'test voice 1', lang: 'lang'})];
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
          createSpeechSynthesisVoice({name: 'test voice 1', lang: 'lang'}),
          createSpeechSynthesisVoice({name: 'test voice 2', lang: 'lang'}),
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
            availableVoices[0]!.name,
            getDropdownItemForVoice(availableVoices[0]!).textContent!.trim());
        assertEquals(
            availableVoices[1]!.name,
            getDropdownItemForVoice(availableVoices[1]!).textContent!.trim());
        assertEquals(2, dropdownItems.length);
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
      selectedVoice =
          createSpeechSynthesisVoice({name: 'test voice 3', lang: 'en-US'});
      previewVoice =
          createSpeechSynthesisVoice({name: 'test voice 1', lang: 'en-US'});

      availableVoices = [
        createSpeechSynthesisVoice({name: 'test voice 0', lang: 'en-US'}),
        previewVoice,
        createSpeechSynthesisVoice({name: 'test voice 2', lang: 'it-IT'}),
        selectedVoice,
      ];
      setAvailableVoices();
    });

    test('it shows a checkmark for the selected voice', () => {
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
      assertEquals(2, groupTitles.length);

      const firstVoice = groupTitles.item(0)!.nextElementSibling!;
      const secondVoice = firstVoice.nextElementSibling!;
      const thirdVoice = secondVoice.nextElementSibling!;
      const italianVoice = groupTitles.item(1)!.nextElementSibling!;
      assertEquals('test voice 0', firstVoice.textContent!.trim());
      assertEquals('test voice 1', secondVoice.textContent!.trim());
      assertEquals('test voice 3', thirdVoice.textContent!.trim());
      assertEquals('test voice 2', italianVoice.textContent!.trim());
    });

    suite('with some disabled languages', () => {
      test('it only shows enabled languages', () => {
        setAvailableVoices(['it-it']);

        const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
        const groupTitles =
            menu.querySelectorAll<HTMLElement>('.lang-group-title');
        assertEquals(1, groupTitles.length);

        const italianVoice = groupTitles.item(0)!.nextElementSibling!;
        assertEquals('test voice 2', italianVoice.textContent!.trim());
      });
    });

    suite('with Natural voices also available', () => {
      setup(() => {
        availableVoices = [
          previewVoice,
          createSpeechSynthesisVoice(
              {name: 'Google US English 1 (Natural)', lang: 'en-US'}),
          createSpeechSynthesisVoice(
              {name: 'Google US English 2 (Natural)', lang: 'en-US'}),
          selectedVoice,
        ];
        setAvailableVoices();
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
            'test voice 1', usEnglishDropdownItems.item(2).textContent!.trim());
        assertEquals(
            'test voice 3', usEnglishDropdownItems.item(3).textContent!.trim());
      });
    });

    suite('with display names for locales', () => {
      let groupTitles: NodeListOf<HTMLElement>;

      setup(() => {
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
        assertEquals('it-IT', groupTitles.item(1)!.textContent!.trim());
      });
    });

    suite('when voices have duplicate names', () => {
      setup(() => {
        availableVoices = [
          createSpeechSynthesisVoice({name: 'English', lang: 'en-US'}),
          createSpeechSynthesisVoice({name: 'English', lang: 'en-US'}),
          createSpeechSynthesisVoice({name: 'English', lang: 'en-UK'}),
        ];
        setAvailableVoices();
      });

      test('it groups the duplicate languages correctly', () => {
        const menu = voiceSelectionMenu!.$.voiceSelectionMenu.get();
        const groupTitles =
            menu.querySelectorAll<HTMLElement>('.lang-group-title');
        const voiceNames = menu.querySelectorAll<HTMLElement>('.voice-name');

        assertEquals(2, groupTitles.length);
        assertEquals('en-US', groupTitles.item(0)!.textContent!.trim());
        assertEquals('en-UK', groupTitles.item(1)!.textContent!.trim());
        assertEquals(3, voiceNames.length);
        assertEquals('English', voiceNames.item(0)!.textContent!.trim());
        assertEquals('English', voiceNames.item(1)!.textContent!.trim());
        assertEquals('English', voiceNames.item(2)!.textContent!.trim());
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
        voiceSelectionMenu.onVoiceSelectionMenuClick(myClickEvent);

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

      suite('when preview finishes playing', () => {
        setup(() => {
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
              'read-anything-20:play-circle', playIconOfPreviewVoice.ironIcon);
          assertEquals('read-anything-20:play-circle', playIconVoice0.ironIcon);
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
      voiceSelectionMenu.voicePackInstallStatus = {
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
      assertEquals(0, getDownloadMessages().length);
    });

    test('no downloading messages with invalid language', () => {
      startDownload('simlish');
      assertEquals(0, getDownloadMessages().length);
    });

    suite('with one language', () => {
      const lang = 'fr';

      setup(() => {
        startDownload(lang);
      });

      test('shows downloading message while installing', () => {
        const msgs = getDownloadMessages();

        assertEquals(1, msgs.length);
        assertStringContains(
            msgs[0]!.textContent!.trim(), 'Downloading Français voices');
      });

      test('hides downloading message when done', () => {
        finishDownload(lang);
        assertEquals(0, getDownloadMessages().length);
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

      test('hides downloading messages when done', () => {
        finishDownload(lang1);
        assertEquals(3, getDownloadMessages().length);

        finishDownload(lang2);
        assertEquals(2, getDownloadMessages().length);

        finishDownload(lang3);
        assertEquals(1, getDownloadMessages().length);

        finishDownload(lang4);
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
