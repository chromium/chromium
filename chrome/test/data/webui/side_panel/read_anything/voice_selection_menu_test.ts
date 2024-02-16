// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {VoiceSelectionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';
import {voiceToHtmlTestId} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('VoiceSelectionMenuElement', () => {
  let voiceSelectionMenu: VoiceSelectionMenuElement;
  let availableVoices: SpeechSynthesisVoice[];

  const setAvailableVoices = () => {
    // Bypass Typescript compiler to allow us to set a private readonly
    // property
    // @ts-ignore
    voiceSelectionMenu.availableVoices = availableVoices;
    flush();
  };

  const getDropdownItemForVoice = (voice: SpeechSynthesisVoice) => {
    return voiceSelectionMenu.$.voiceSelectionMenu
        .querySelector<HTMLButtonElement>(`[data-test-id="${
            voiceToHtmlTestId(voice)}"].dropdown-voice-selection-button`)!;
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    voiceSelectionMenu = document.createElement('voice-selection-menu');
    document.body.appendChild(voiceSelectionMenu);
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
      const button =
          voiceSelectionMenu.shadowRoot!.querySelector<CrIconButtonElement>(
              '#voice-selection');
      button!.click();

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
        const button =
            voiceSelectionMenu.shadowRoot!.querySelector<CrIconButtonElement>(
                '#voice-selection')!;
        button!.click();

        const dropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu.$.voiceSelectionMenu
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
          voiceSelectionMenu.$.voiceSelectionMenu.querySelector<HTMLElement>(
              'div#group-en-US')!;
      const italianGroup: HTMLElement =
          voiceSelectionMenu.$.voiceSelectionMenu.querySelector<HTMLElement>(
              'div#group-it-IT')!;

      const englishDropdownItems: NodeListOf<HTMLElement> =
          englishGroup.querySelectorAll<HTMLButtonElement>(
              '.dropdown-voice-selection-button');
      const italianDropdownItems: NodeListOf<HTMLElement> =
          italianGroup.querySelectorAll<HTMLButtonElement>(
              '.dropdown-voice-selection-button');

      assertEquals(englishDropdownItems.length, 3);
      assertEquals(italianDropdownItems.length, 1);
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
            voiceSelectionMenu.$.voiceSelectionMenu.querySelector<HTMLElement>(
                'div#group-en-US')!;
        const ukEnglishGroup: HTMLElement =
            voiceSelectionMenu.$.voiceSelectionMenu.querySelector<HTMLElement>(
                'div#group-en-UK')!;

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
        // Click button to display dropdown menu
        const button =
            voiceSelectionMenu.shadowRoot!.querySelector<CrIconButtonElement>(
                '#voice-selection')!;
        button!.click();

        // Bypass Typescript compiler to allow us to set a private readonly
        // property
        // @ts-ignore
        voiceSelectionMenu.previewVoicePlaying = previewVoice;
        flush();
      });

      test('it shows preview-playing button when preview plays', () => {
        const playIconVoice0 = getDropdownItemForVoice(availableVoices[0]!)
                                   .querySelector<HTMLElement>('#play-icon')!;
        const pauseIconVoice0 = getDropdownItemForVoice(availableVoices[0]!)
                                    .querySelector<HTMLElement>('#pause-icon')!;
        const playIconOfPreviewVoice =
            getDropdownItemForVoice(previewVoice)
                .querySelector<HTMLElement>('#play-icon')!;
        const pauseIconOfPreviewVoice =
            getDropdownItemForVoice(previewVoice)
                .querySelector<HTMLElement>('#pause-icon')!;

        // The play icon should flip to pause for the voice being previewed
        assertFalse(isPositionedOnPage(playIconOfPreviewVoice));
        assertTrue(isPositionedOnPage(pauseIconOfPreviewVoice));
        // The play icon should remain for the other buttons
        assertTrue(isPositionedOnPage(playIconVoice0));
        assertFalse(isPositionedOnPage(pauseIconVoice0));
      });

      suite('when preview finishes playing', () => {
        setup(() => {
          // Bypass Typescript compiler to allow us to set a private readonly
          // property
          // @ts-ignore
          voiceSelectionMenu.previewVoicePlaying = null;
          flush();
        });

        test('it flips the preview pause button back to play', () => {
          const playIconVoice0 = getDropdownItemForVoice(availableVoices[0]!)
                                     .querySelector<HTMLElement>('#play-icon')!;
          const pauseIconVoice0 =
              getDropdownItemForVoice(availableVoices[0]!)
                  .querySelector<HTMLElement>('#pause-icon')!;
          const playIconOfPreviewVoice =
              getDropdownItemForVoice(availableVoices[1]!)
                  .querySelector<HTMLElement>('#play-icon')!;
          const pauseIconOfPreviewVoice =
              getDropdownItemForVoice(availableVoices[1]!)
                  .querySelector<HTMLElement>('#pause-icon')!;

          // All icons should be play icons because no preview is playing
          assertTrue(isPositionedOnPage(playIconOfPreviewVoice));
          assertFalse(isPositionedOnPage(pauseIconOfPreviewVoice));
          assertTrue(isPositionedOnPage(playIconVoice0));
          assertFalse(isPositionedOnPage(pauseIconVoice0));
        });
      });
    });
  });
});

function isHiddenWithCss(element: HTMLElement): boolean {
  return window.getComputedStyle(element).visibility === 'hidden';
}

function isPositionedOnPage(element: HTMLElement) {
  return !!(
      element.offsetWidth || element.offsetHeight ||
      element.getClientRects().length);
}
