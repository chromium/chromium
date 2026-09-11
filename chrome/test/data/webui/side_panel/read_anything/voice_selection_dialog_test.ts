// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrRadioButtonElement} from '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import type {CrRadioGroupElement} from '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {VoiceSelectionDialogElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {stringToHtmlTestId, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, setupTestEnvironment} from './common.js';

suite('VoiceSelectionDialog', () => {
  let dialog: VoiceSelectionDialogElement;

  const googleNaturalVoice = createSpeechSynthesisVoice(
      {name: 'Google US English (Natural)', lang: 'en-US'});
  const googleStandardVoice =
      createSpeechSynthesisVoice({name: 'Google US English', lang: 'en-US'});
  const googleItalianVoice =
      createSpeechSynthesisVoice({name: 'Google Italian Voice', lang: 'it-IT'});

  async function setAvailableVoicesAndEnabledLangs(
      availableVoices: SpeechSynthesisVoice[],
      enabledLangs?: string[]): Promise<void> {
    dialog.availableVoices = availableVoices;
    if (enabledLangs === undefined) {
      dialog.enabledLangs =
          [...new Set(availableVoices.map(({lang}) => lang.toLowerCase()))];
    } else {
      dialog.enabledLangs = enabledLangs.map(l => l.toLowerCase());
    }
    return microtasksFinished();
  }

  function getDropdownItemForVoice(voice: SpeechSynthesisVoice):
      CrRadioButtonElement {
    return dialog.$.voiceSelectionDialog.querySelector<CrRadioButtonElement>(
        `cr-radio-button[data-test-id="${stringToHtmlTestId(voice.name)}"]`)!;
  }

  function getRadioGroup(): CrRadioGroupElement {
    return dialog.$.voiceSelectionDialog.querySelector<CrRadioGroupElement>(
        '#voiceRadioGroup')!;
  }

  function getGroupTitles(): NodeListOf<HTMLElement> {
    return dialog.$.voiceSelectionDialog.querySelectorAll<HTMLElement>(
        '.lang-group-title');
  }

  async function drawDialog(): Promise<void> {
    document.body.appendChild(dialog);
    return microtasksFinished();
  }

  setup(async () => {
    setupTestEnvironment();
    loadTimeData.overrideValues({
      cancel: 'Cancel',
      save: 'Save',
      voiceSelectionLabel: 'Voice Selection',
      previewTooltip: 'Preview voice',
      stopLabel: 'Stop preview',
      readingModeLanguageMenuItemLabel: 'Select {0}',
    });

    dialog = document.createElement('voice-selection-dialog');
    dialog.localeToDisplayName = {
      'en-us': 'English (United States)',
      'it-it': 'Italian',
    };
    await setAvailableVoicesAndEnabledLangs(
        [googleNaturalVoice, googleStandardVoice, googleItalianVoice]);
    dialog.selectedVoice = googleNaturalVoice;
  });

  test('does not render close icon button in dialog header', async () => {
    await drawDialog();
    const closeBtn =
        dialog.$.voiceSelectionDialog.shadowRoot.querySelector<HTMLElement>(
            '#close');

    assertFalse(!!closeBtn);
  });

  test('renders Save and Cancel action buttons in footer', async () => {
    await drawDialog();
    const cancelButton =
        dialog.$.voiceSelectionDialog.querySelector<CrButtonElement>(
            '#cancelButton');
    const saveButton =
        dialog.$.voiceSelectionDialog.querySelector<CrButtonElement>(
            '#saveButton');

    assertTrue(!!cancelButton);
    assertTrue(!!saveButton);
  });

  test('groups voices by language and sorts natural voices first', async () => {
    await drawDialog();
    const titles = getGroupTitles();

    assertEquals(2, titles.length);
    assertEquals('English (United States)', titles[0]!.textContent.trim());
    assertEquals('Italian', titles[1]!.textContent.trim());

    const englishNaturalItem = getDropdownItemForVoice(googleNaturalVoice);
    const englishStandardItem = getDropdownItemForVoice(googleStandardVoice);
    const italianItem = getDropdownItemForVoice(googleItalianVoice);

    assertTrue(!!englishNaturalItem);
    assertTrue(!!englishStandardItem);
    assertTrue(!!italianItem);

    assertEquals(
        googleNaturalVoice.name,
        englishNaturalItem.querySelector('.voice-name')!.textContent.trim());
    assertEquals(
        googleStandardVoice.name,
        englishStandardItem.querySelector('.voice-name')!.textContent.trim());
    assertEquals(
        googleItalianVoice.name,
        italianItem.querySelector('.voice-name')!.textContent.trim());
  });

  test(
      'enforces cr-radio-group selected state and checked properties',
      async () => {
        await drawDialog();
        const radioGroup = getRadioGroup();

        assertEquals(googleNaturalVoice.name, radioGroup.selected);

        const selectedItem = getDropdownItemForVoice(googleNaturalVoice);
        const unselectedItem = getDropdownItemForVoice(googleStandardVoice);

        assertTrue(selectedItem.checked);
        assertFalse(unselectedItem.checked);
      });

  test(
      'clicking radio button stages candidate selection without firing event',
      async () => {
        await drawDialog();
        let voiceFired = false;
        dialog.addEventListener(ToolbarEvent.VOICE, () => {
          voiceFired = true;
        });

        const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
        standardVoiceItem.click();
        await microtasksFinished();

        const naturalItem = getDropdownItemForVoice(googleNaturalVoice);
        const updatedStandardItem =
            getDropdownItemForVoice(googleStandardVoice);

        assertTrue(updatedStandardItem.checked);
        assertFalse(naturalItem.checked);
        assertEquals(googleStandardVoice.name, getRadioGroup().selected);
        assertFalse(voiceFired);
        assertTrue(dialog.$.voiceSelectionDialog.open);
      });

  test('Space and Enter keys stage candidate voices', async () => {
    await drawDialog();

    const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
    standardVoiceItem.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Enter', bubbles: true, composed: true}));
    await microtasksFinished();

    assertTrue(standardVoiceItem.checked);
    assertEquals(googleStandardVoice.name, getRadioGroup().selected);

    const naturalItem = getDropdownItemForVoice(googleNaturalVoice);
    naturalItem.dispatchEvent(new KeyboardEvent(
        'keydown', {key: ' ', bubbles: true, composed: true}));
    await microtasksFinished();

    assertTrue(naturalItem.checked);
    assertEquals(googleNaturalVoice.name, getRadioGroup().selected);
  });

  test('clicking Save commits candidate voice and fires event', async () => {
    await drawDialog();

    const voiceEventPromise =
        eventToPromise<CustomEvent<{selectedVoice: SpeechSynthesisVoice}>>(
            ToolbarEvent.VOICE, dialog);

    const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
    standardVoiceItem.click();
    await microtasksFinished();

    const saveBtn =
        dialog.$.voiceSelectionDialog.querySelector<CrButtonElement>(
            '#saveButton')!;
    saveBtn.click();
    const event = await voiceEventPromise;

    assertEquals(googleStandardVoice.name, event.detail.selectedVoice.name);
    assertFalse(dialog.$.voiceSelectionDialog.open);
  });

  test('clicking Save without changes does not fire event', async () => {
    await drawDialog();

    let voiceFired = false;
    dialog.addEventListener(ToolbarEvent.VOICE, () => {
      voiceFired = true;
    });

    const saveBtn =
        dialog.$.voiceSelectionDialog.querySelector<CrButtonElement>(
            '#saveButton')!;
    saveBtn.click();
    await microtasksFinished();

    assertFalse(voiceFired);
  });

  test('clicking Cancel discards staged voice and closes dialog', async () => {
    await drawDialog();

    let voiceFired = false;
    dialog.addEventListener(ToolbarEvent.VOICE, () => {
      voiceFired = true;
    });

    const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
    standardVoiceItem.click();
    await microtasksFinished();

    const cancelBtn =
        dialog.$.voiceSelectionDialog.querySelector<CrButtonElement>(
            '#cancelButton')!;
    cancelBtn.click();
    await microtasksFinished();

    assertFalse(voiceFired);
    assertFalse(dialog.$.voiceSelectionDialog.open);
  });

  test('escape key cancels dialog and rolls back candidate state', async () => {
    await drawDialog();

    const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
    standardVoiceItem.click();
    await microtasksFinished();

    dialog.$.voiceSelectionDialog.dispatchEvent(new Event('cancel'));
    dialog.close();
    await microtasksFinished();

    const naturalItem = getDropdownItemForVoice(googleNaturalVoice);

    assertTrue(naturalItem.checked);
  });

  test('clicking preview button does not select the radio item', async () => {
    await drawDialog();

    const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
    const previewBtn =
        standardVoiceItem.querySelector<CrIconButtonElement>('#preview-icon')!;

    previewBtn.click();
    await microtasksFinished();

    const naturalItem = getDropdownItemForVoice(googleNaturalVoice);

    assertTrue(naturalItem.checked);
    assertFalse(standardVoiceItem.checked);
  });

  test('closing dialog fires close event', async () => {
    await drawDialog();
    const closePromise = eventToPromise('close', dialog);
    dialog.close();
    await closePromise;
  });

  test(
      'roving tabindex on preview button falls back to first visible voice' +
          ' when candidate is missing',
      async () => {
        dialog.selectedVoice =
            createSpeechSynthesisVoice({name: 'Missing Voice', lang: 'fr-FR'});
        await drawDialog();

        const naturalItem = getDropdownItemForVoice(googleNaturalVoice);
        const standardItem = getDropdownItemForVoice(googleStandardVoice);
        const naturalPreview =
            naturalItem.querySelector<CrIconButtonElement>('#preview-icon')!;

        assertEquals('0', naturalPreview.getAttribute('tabindex'));

        const standardPreview =
            standardItem.querySelector<CrIconButtonElement>('#preview-icon')!;

        assertEquals('-1', standardPreview.getAttribute('tabindex'));
      });
});
