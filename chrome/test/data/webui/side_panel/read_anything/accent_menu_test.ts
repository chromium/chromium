// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AccentMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, setupTestEnvironment} from './common.js';

suite('AccentMenu', () => {
  let accentMenu: AccentMenuElement;
  let availableVoices: SpeechSynthesisVoice[];

  function getAccentButtons(): NodeListOf<HTMLButtonElement> {
    return accentMenu.$.accentMenu.querySelectorAll<HTMLButtonElement>(
        '.dropdown-item');
  }

  function getSearchField() {
    return accentMenu.$.searchField;
  }

  setup(async () => {
    setupTestEnvironment();
    accentMenu = document.createElement('accent-menu');
    accentMenu.localesOfLangPackVoices = new Set(['en-us', 'en-gb']);
    document.body.appendChild(accentMenu);

    availableVoices = [
      createSpeechSynthesisVoice({name: 'Voice 1', lang: 'en-US'}),
      createSpeechSynthesisVoice({name: 'Voice 2', lang: 'en-GB'}),
    ];
    accentMenu.availableVoices = availableVoices;
    accentMenu.localeToDisplayName = {
      'en-us': 'English (United States)',
      'en-gb': 'English (United Kingdom)',
    };
    accentMenu.selectedLang = 'en-us';
    await microtasksFinished();
  });

  test('renders accent list with selected item marked', () => {
    const buttons = getAccentButtons();
    assertTrue(buttons.length >= 2);

    const checkMarks =
        accentMenu.$.accentMenu.querySelectorAll<HTMLElement>('.check-mark');
    assertTrue(checkMarks.length >= 2);

    // en-gb is not selected -> check-mark-showing-false
    assertTrue(checkMarks[0]!.classList.contains('check-mark-showing-false'));
    // en-us is selected -> check-mark-showing-true
    assertTrue(checkMarks[1]!.classList.contains('check-mark-showing-true'));
  });

  test('clears search on clear button click', async () => {
    getSearchField().value = 'United Kingdom';
    await microtasksFinished();
    assertEquals(1, getAccentButtons().length);

    const clearButton = accentMenu.$.accentMenu.querySelector<HTMLElement>(
        '#clearLanguageSearch');
    assertTrue(!!clearButton);
    clearButton.click();
    await microtasksFinished();

    assertEquals('', getSearchField().value);
    assertTrue(getAccentButtons().length >= 2);
  });
});
