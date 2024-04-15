// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {VoiceSelectionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

function stringToHtmlTestId(s: string): string {
  return s.replace(/\s/g, '-').replace(/[()]/g, '');
}

suite('VoiceSelectionMenuElement', () => {
  let voiceSelectionMenu: VoiceSelectionMenuElement|null;
  let availableVoices: SpeechSynthesisVoice[];
  let myClickEvent: MouseEvent;

  const setAvailableVoices = () => {
    // Bypass Typescript compiler to allow us to set a private readonly
    // property
    // @ts-ignore
    voiceSelectionMenu.availableVoices = availableVoices;
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

    flush();
  });

  suite('with one voice', () => {
    setup(() => {
      availableVoices = [{name: 'test voice 1'} as SpeechSynthesisVoice];
      setAvailableVoices();
    });

    test('it does not show dropdown before click', () => {
      assertFalse(
          isPositionedOnPage(getDropdownItemForVoice(availableVoices[0]!)));
    });

    test('it shows dropdown items after button click', () => {
      voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);

      flush();

      assertTrue(
          isPositionedOnPage(getDropdownItemForVoice(availableVoices[0]!)!));
      assertEquals(
          getDropdownItemForVoice(availableVoices[0]!)!.textContent!.trim(),
          availableVoices[0]!.name);
    });

    suite('when availableVoices updates', () => {
      setup(() => {
        availableVoices = [
          {name: 'test voice 1'} as SpeechSynthesisVoice,
          {name: 'test voice 2'} as SpeechSynthesisVoice,
        ];
        setAvailableVoices();
      });

      test('it updates and displays the new voices', () => {
        voiceSelectionMenu!.onVoiceSelectionMenuClick(myClickEvent);
        flush();

        const dropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelectorAll<HTMLButtonElement>(
                    '.dropdown-voice-selection-button');

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
      const englishGroup: HTMLElement =
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelector<HTMLElement>('div[data-test-id="group-en-US"]')!;
      const italianGroup: HTMLElement =
          voiceSelectionMenu!.$.voiceSelectionMenu.get()
              .querySelector<HTMLElement>('div[data-test-id="group-it-IT"]')!;

      const englishDropdownItems: NodeListOf<HTMLElement> =
          englishGroup.querySelectorAll<HTMLButtonElement>(
              '.dropdown-voice-selection-button');
      const italianDropdownItems: NodeListOf<HTMLElement> =
          italianGroup.querySelectorAll<HTMLButtonElement>(
              '.dropdown-voice-selection-button');

      assertEquals(englishDropdownItems.length, 3);
      assertEquals(italianDropdownItems.length, 1);
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
        const englishGroup: HTMLElement =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelector<HTMLElement>('div[data-test-id="group-en-US"]')!;
        const usEnglishDropdownItems: NodeListOf<HTMLElement> =
            englishGroup.querySelectorAll('.voice-name');

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
      setup(() => {
        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        voiceSelectionMenu.localeToDisplayName = {
          'en-US': 'English (United States)',
        };
        flush();
      });

      test('it displays the display name', () => {
        const englishGroup: HTMLElement =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelector<HTMLElement>(
                    'div[data-test-id="group-English-United-States"]')!;
        const groupNameSpan = englishGroup.querySelector<HTMLElement>('span');

        assertEquals(
            groupNameSpan!.textContent!.trim(), 'English (United States)');
      });

      test('it defaults to the locale when there is no display name', () => {
        const italianGroup: HTMLElement =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelector<HTMLElement>('div[data-test-id="group-it-IT"]')!;
        const groupNameSpan = italianGroup.querySelector<HTMLElement>('span');

        assertEquals(groupNameSpan!.textContent!.trim(), 'it-IT');
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
        const usEnglishGroup: HTMLElement =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelector<HTMLElement>('div[data-test-id="group-en-US"]')!;
        const ukEnglishGroup: HTMLElement =
            voiceSelectionMenu!.$.voiceSelectionMenu.get()
                .querySelector<HTMLElement>('div[data-test-id="group-en-UK"]')!;

        const usEnglishDropdownItems: NodeListOf<HTMLElement> =
            usEnglishGroup.querySelectorAll<HTMLButtonElement>(
                '.dropdown-voice-selection-button');
        const ukEnglishDropdownItems: NodeListOf<HTMLElement> =
            ukEnglishGroup.querySelectorAll<HTMLButtonElement>(
                '.dropdown-voice-selection-button');

        assertEquals(usEnglishDropdownItems.length, 2);
        assertEquals(ukEnglishDropdownItems.length, 1);
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
        const playIconVoice0 =
            getDropdownItemForVoice(availableVoices[0]!)
                .querySelector<HTMLButtonElement>('#play-icon')!;
        const playIconOfPreviewVoice =
            getDropdownItemForVoice(previewVoice)
                .querySelector<HTMLButtonElement>('#play-icon')!;

        // The play icon should flip to disabled for the voice being previewed
        assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
        assertTrue(isDisabled(playIconOfPreviewVoice));
        // The play icon should remain enabled for the other buttons
        assertTrue(isPositionedOnPage(playIconVoice0));
        assertFalse(isDisabled(playIconVoice0));
      });

      suite('when preview finishes playing', () => {
        setup(() => {
          // Bypass Typescript compiler to allow us to set a private readonly
          // property
          // @ts-ignore
          voiceSelectionMenu.previewVoicePlaying = null;
          flush();
        });

        test('it flips the preview button back to enabled', () => {
          const playIconVoice0 =
              getDropdownItemForVoice(availableVoices[0]!)
                  .querySelector<HTMLButtonElement>('#play-icon')!;
          const playIconOfPreviewVoice =
              getDropdownItemForVoice(availableVoices[1]!)
                  .querySelector<HTMLButtonElement>('#play-icon')!;

          // All icons should be enabled play icons because no preview is
          // playing
          assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
          assertTrue(isPositionedOnPage(playIconVoice0));
          assertFalse(isDisabled(playIconVoice0));
          assertFalse(isDisabled(playIconOfPreviewVoice));
        });
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

function isDisabled(element: HTMLButtonElement) {
  return element.disabled;
}
