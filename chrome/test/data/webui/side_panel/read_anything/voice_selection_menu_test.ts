// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {VoiceSelectionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_selection_menu.js';
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
      const dropdownItems: NodeListOf<HTMLElement> =
          voiceSelectionMenu.$.voiceSelectionMenu
              .querySelectorAll<HTMLButtonElement>('.dropdown-item');

      assertFalse(isVisible(dropdownItems.item(0)));
    });

    test('it shows dropdown items after button click', () => {
      const button =
          voiceSelectionMenu.shadowRoot!.querySelector<CrIconButtonElement>(
              '#voice-selection');
      button!.click();
      flush();

      const dropdownItems: NodeListOf<HTMLElement> =
          voiceSelectionMenu.$.voiceSelectionMenu
              .querySelectorAll<HTMLButtonElement>('.dropdown-item');

      assertTrue(isVisible(dropdownItems.item(0)!));
      assertEquals(
          dropdownItems.item(0)!.textContent!.trim(), availableVoices[0]!.name);
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
        flush();

        const dropdownItems: NodeListOf<HTMLElement> =
            voiceSelectionMenu.$.voiceSelectionMenu
                .querySelectorAll<HTMLButtonElement>('.dropdown-item');

        assertEquals(
            dropdownItems.item(0)!.textContent!.trim(),
            availableVoices[0]!.name);
        assertEquals(
            dropdownItems.item(1)!.textContent!.trim(),
            availableVoices[1]!.name);
        assertEquals(dropdownItems.length, 2);
        assertTrue(isVisible(dropdownItems.item(0)!));
        assertTrue(isVisible(dropdownItems.item(1)!));
      });
    });
  });
});

function isVisible(element: HTMLElement) {
  return !!(
      element.offsetWidth || element.offsetHeight ||
      element.getClientRects().length);
}
