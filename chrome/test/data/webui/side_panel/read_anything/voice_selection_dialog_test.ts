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
import {AudioBrowserProxyImpl, getVoiceNatureNaming, ReadAloudSettingsChange, spinnerDebounceTimeout, stringToHtmlTestId, ToolbarEvent, VoiceClientSideStatusCode, VoiceNotificationManager} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, setupTestEnvironment} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('VoiceSelectionDialog', () => {
  let dialog: VoiceSelectionDialogElement;
  let metrics: TestMetricsBrowserProxy;

  const googleNaturalVoice = createSpeechSynthesisVoice(
      {name: 'Google US English (Natural)', lang: 'en-US'});
  const googleStandardVoice =
      createSpeechSynthesisVoice({name: 'Google US English', lang: 'en-US'});
  const googleItalianVoice =
      createSpeechSynthesisVoice({name: 'Google Italian Voice', lang: 'it-IT'});
  // Key for getVoiceNatureNaming. googleNaturalVoice can't be used as key
  // due to the missing voice number.
  const mappedVoice = createSpeechSynthesisVoice(
      {name: 'Google US English 1 (Natural)', lang: 'en-US'});
  const mappedSpanishVoice = createSpeechSynthesisVoice(
      {name: 'Google español de Estados Unidos 1 (Natural)', lang: 'es-US'});

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
    const result = setupTestEnvironment();
    metrics = result.metrics;

    const audioBrowserProxy =
        AudioBrowserProxyImpl.getInstance() as TestAudioBrowserProxy;
    audioBrowserProxy.localeToDisplayName = {
      'it': 'Italian',
      'it-it': 'Italian',
      'en-us': 'English (United States)',
    };

    loadTimeData.overrideValues({
      cancel: 'Cancel',
      save: 'Save',
      voiceSelectionLabel: 'Voice Selection',
      previewTooltip: 'Preview voice',
      stopLabel: 'Stop preview',
      readingModeLanguageMenuItemLabel: 'Select $1',
      readingModeVoiceMenuDownloading: 'Downloading $1',
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
      'preview button roving tabindex sets 0 on candidate voice and -1 on' +
          ' others',
      async () => {
        await drawDialog();

        const selectedItem = getDropdownItemForVoice(googleNaturalVoice);
        const unselectedItem = getDropdownItemForVoice(googleStandardVoice);

        const selectedPreview =
            selectedItem.querySelector<CrIconButtonElement>('#preview-icon')!;
        const unselectedPreview =
            unselectedItem.querySelector<CrIconButtonElement>('#preview-icon')!;

        assertEquals('0', selectedPreview.getAttribute('tabindex'));
        assertEquals('-1', unselectedPreview.getAttribute('tabindex'));
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
    assertEquals(0, metrics.getCallCount('recordSpeechSettingsChange'));
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
    assertEquals(0, metrics.getCallCount('recordSpeechSettingsChange'));
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

  test(
      'arrow keys navigate and stage candidate voices via cr-radio-group',
      async () => {
        await drawDialog();

        const naturalItem = getDropdownItemForVoice(googleNaturalVoice);

        assertTrue(naturalItem.checked);
        assertEquals(googleNaturalVoice.name, getRadioGroup().selected);

        naturalItem.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
        await microtasksFinished();
        const standardItem = getDropdownItemForVoice(googleStandardVoice);

        assertTrue(standardItem.checked);
        assertFalse(naturalItem.checked);
        assertEquals(googleStandardVoice.name, getRadioGroup().selected);

        standardItem.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
        await microtasksFinished();

        assertTrue(naturalItem.checked);
        assertFalse(standardItem.checked);
        assertEquals(googleNaturalVoice.name, getRadioGroup().selected);
      });

  test(
      'clicking preview button schedules preview and toggles to stop icon',
      async () => {
        await drawDialog();

        const playPreviewPromise = eventToPromise<
            CustomEvent<{previewVoice: SpeechSynthesisVoice | null}>>(
            ToolbarEvent.PLAY_PREVIEW, dialog);
        const standardItem = getDropdownItemForVoice(googleStandardVoice);
        const previewBtn =
            standardItem.querySelector<CrIconButtonElement>('#preview-icon')!;
        const mockTimer = new MockTimer();
        mockTimer.install();
        previewBtn.click();
        const event = await playPreviewPromise;

        assertEquals(googleStandardVoice.name, event.detail.previewVoice!.name);

        mockTimer.tick(spinnerDebounceTimeout);
        mockTimer.uninstall();
        await microtasksFinished();

        const updatedPreviewBtn =
            standardItem.querySelector<CrIconButtonElement>('#preview-icon')!;

        assertEquals(
            'read-anything-20:stop-circle', updatedPreviewBtn.ironIcon);
      });

  test(
      'clicking preview button while pending debounce cancels preview',
      async () => {
        await drawDialog();

        const playPreviewPayloads: Array<SpeechSynthesisVoice|null> = [];
        dialog.addEventListener(ToolbarEvent.PLAY_PREVIEW, (event: Event) => {
          const e =
              event as CustomEvent<{previewVoice: SpeechSynthesisVoice | null}>;
          playPreviewPayloads.push(e.detail.previewVoice);
        });

        const standardItem = getDropdownItemForVoice(googleStandardVoice);
        const previewBtn =
            standardItem.querySelector<CrIconButtonElement>('#preview-icon')!;

        const mockTimer = new MockTimer();
        mockTimer.install();

        // First click: initiates preview request
        previewBtn.click();

        assertEquals(1, playPreviewPayloads.length);
        assertEquals(googleStandardVoice.name, playPreviewPayloads[0]!.name);

        // Second click before debounce expires: cancels preview
        previewBtn.click();

        assertEquals(2, playPreviewPayloads.length);
        assertEquals(null, playPreviewPayloads[1]);

        mockTimer.tick(spinnerDebounceTimeout);
        mockTimer.uninstall();
        await microtasksFinished();

        // Verify stop icon never rendered because debounce was cancelled
        assertEquals('read-anything-20:play-circle', previewBtn.ironIcon);
      });

  test('closing dialog stops active preview', async () => {
    await drawDialog();

    const standardItem = getDropdownItemForVoice(googleStandardVoice);
    const previewBtn =
        standardItem.querySelector<CrIconButtonElement>('#preview-icon')!;

    const mockTimer = new MockTimer();
    mockTimer.install();
    previewBtn.click();
    mockTimer.tick(spinnerDebounceTimeout);
    mockTimer.uninstall();
    await microtasksFinished();

    const stopPreviewPromise = eventToPromise<
        CustomEvent<{previewVoice: SpeechSynthesisVoice | null}>>(
        ToolbarEvent.PLAY_PREVIEW, dialog);

    dialog.close();
    const event = await stopPreviewPromise;

    assertEquals(null, event.detail.previewVoice);
  });

  test('clicking Save logs UMA speech settings change metric', async () => {
    await drawDialog();

    const standardVoiceItem = getDropdownItemForVoice(googleStandardVoice);
    standardVoiceItem.click();
    await microtasksFinished();

    const saveBtn =
        dialog.$.voiceSelectionDialog.querySelector<CrButtonElement>(
            '#saveButton')!;
    saveBtn.click();
    await microtasksFinished();

    assertEquals(
        ReadAloudSettingsChange.VOICE_NAME_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
  });

  test(
      'displays download notifications from notification manager', async () => {
        await drawDialog();

        VoiceNotificationManager.getInstance().onVoiceStatusChange(
            'it', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
            [googleItalianVoice], true);
        await microtasksFinished();

        const downloadMessage =
            dialog.$.voiceSelectionDialog.querySelector<HTMLElement>(
                '.download-message')!;

        assertTrue(downloadMessage.textContent.includes('Italian'));
      });

  test(
      'renders the nature name and descriptor for a mapped voice',
      async () => {
        await setAvailableVoicesAndEnabledLangs(
            [mappedVoice, mappedSpanishVoice]);
        await drawDialog();

        for (const voice of [mappedVoice, mappedSpanishVoice]) {
          const expected = getVoiceNatureNaming(voice.name)!;
          const row = getDropdownItemForVoice(voice);
          const nameSpan = row.querySelector<HTMLElement>('.voice-name')!;
          const descriptionSpan =
              row.querySelector<HTMLElement>('.voice-description')!;

          assertEquals(expected.natureName, nameSpan.textContent);
          assertEquals(expected.description, descriptionSpan.textContent);
          // The nature name duplicates the accessible name, so it is hidden
          // from assistive technology; the descriptor is not, because it
          // contributes to the row's accessible description.
          assertEquals('true', nameSpan.getAttribute('aria-hidden'));
          assertFalse(descriptionSpan.hasAttribute('aria-hidden'));
        }
      });

  test('synthesizes no joiner between the two identity lines', async () => {
    await setAvailableVoicesAndEnabledLangs([mappedVoice, mappedSpanishVoice]);
    await drawDialog();

    for (const voice of [mappedVoice, mappedSpanishVoice]) {
      const expected = getVoiceNatureNaming(voice.name)!;
      const row = getDropdownItemForVoice(voice);
      const text = row.textContent;
      const nameIndex = text.indexOf(expected.natureName);
      const descriptionIndex = text.indexOf(expected.description);

      assertTrue(nameIndex >= 0);
      assertTrue(descriptionIndex > nameIndex);

      assertEquals(
          '',
          text.substring(
                  nameIndex + expected.natureName.length, descriptionIndex)
              .trim());
    }
  });

  test('labels a mapped row with the nature name, still wrapped', async () => {
    await setAvailableVoicesAndEnabledLangs([mappedVoice, mappedSpanishVoice]);
    await drawDialog();

    for (const voice of [mappedVoice, mappedSpanishVoice]) {
      const expected = getVoiceNatureNaming(voice.name)!;
      const row = getDropdownItemForVoice(voice);

      assertEquals(`Select ${expected.natureName}`, row.getAttribute('label'));
    }
  });

  test('falls back to the engine name when a voice is unmapped', async () => {
    await setAvailableVoicesAndEnabledLangs([googleNaturalVoice]);
    await drawDialog();

    const row = getDropdownItemForVoice(googleNaturalVoice);
    const nameSpan = row.querySelector<HTMLElement>('.voice-name')!;

    assertEquals(googleNaturalVoice.name, nameSpan.textContent);
    assertEquals(
        `Select ${googleNaturalVoice.name}`, row.getAttribute('label'));
    // Absent from the DOM, not present and empty.
    assertEquals(null, row.querySelector('.voice-description'));
  });

  test('tags both identity lines with the voice language', async () => {
    await setAvailableVoicesAndEnabledLangs([mappedVoice, mappedSpanishVoice]);
    await drawDialog();

    for (const voice of [mappedVoice, mappedSpanishVoice]) {
      const row = getDropdownItemForVoice(voice);
      const nameSpan = row.querySelector<HTMLElement>('.voice-name')!;
      const descriptionSpan =
          row.querySelector<HTMLElement>('.voice-description')!;

      // Emitted verbatim: the engine's value is never normalised or validated.
      assertEquals(voice.lang, nameSpan.getAttribute('lang'));
      assertEquals(voice.lang, descriptionSpan.getAttribute('lang'));
    }
  });

  test('leaves an unmapped row untagged despite a valid lang', async () => {
    await setAvailableVoicesAndEnabledLangs([googleNaturalVoice]);
    await drawDialog();

    const row = getDropdownItemForVoice(googleNaturalVoice);
    const nameSpan = row.querySelector<HTMLElement>('.voice-name')!;

    // googleNaturalVoice.lang is 'en-US', so this is not about the value
    // being unusable. An unmapped row shows the engine's own string, which
    // is predominantly English with an embedded native fragment; claiming it
    // is written in the voice's locale asserts something we do not know.
    assertFalse(nameSpan.hasAttribute('lang'));
  });

  test('tags only the identity spans, never the row host', async () => {
    await setAvailableVoicesAndEnabledLangs([
      mappedVoice,
      mappedSpanishVoice,
      googleNaturalVoice,
      googleItalianVoice,
    ]);
    await drawDialog();

    const rows =
        dialog.$.voiceSelectionDialog.querySelectorAll<CrRadioButtonElement>(
            'cr-radio-button');

    assertEquals(4, rows.length);
    for (const row of rows) {
      // The accessible name is bilingual -- nature name plus an English
      // instruction -- so no single lang can describe it, and tagging the
      // host would apply one to both halves.
      assertFalse(row.hasAttribute('lang'));

      for (const tagged of row.querySelectorAll('[lang]')) {
        assertTrue(
            tagged.classList.contains('voice-name') ||
                tagged.classList.contains('voice-description'),
            `unexpected lang on ${tagged.className}`);
      }
    }
  });

  test('saving a mapped row still commits the engine voice', async () => {
    await setAvailableVoicesAndEnabledLangs([mappedVoice, googleStandardVoice]);
    dialog.selectedVoice = googleStandardVoice;
    await drawDialog();

    const voiceEventPromise =
        eventToPromise<CustomEvent<{selectedVoice: SpeechSynthesisVoice}>>(
            ToolbarEvent.VOICE, dialog);

    getDropdownItemForVoice(mappedVoice).click();
    await microtasksFinished();

    dialog.$.voiceSelectionDialog
        .querySelector<CrButtonElement>('#saveButton')!.click();
    const event = await voiceEventPromise;

    // The display name is the nature name; the identity is not.
    assertEquals(mappedVoice.name, event.detail.selectedVoice.name);
    assertEquals(
        ReadAloudSettingsChange.VOICE_NAME_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
  });
});
