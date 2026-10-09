// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {AudioMenuElement, VoiceSelectionDialogElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {getVoiceNatureNaming, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, createSpeechSynthesisVoice, getItemsInMenu, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';

suite('AudioMenuElement', () => {
  let audioMenu: AudioMenuElement;
  let audioBrowserProxy: TestAudioBrowserProxy;

  function createAudioMenu() {
    audioMenu = document.createElement('audio-menu');
    document.body.appendChild(audioMenu);
  }

  setup(() => {
    const result = setupTestEnvironment();
    audioBrowserProxy = result.audioBrowserProxy;
  });

  test('has checkmarks', () => {
    createAudioMenu();
    assertCheckMarksForDropdown(audioMenu);
  });

  test('highlight granularity prop update changes selected item', async () => {
    createAudioMenu();
    const wordHighlight = audioBrowserProxy.getWordHighlighting();
    audioMenu.highlightGranularity = wordHighlight;
    await microtasksFinished();

    const selectedItems =
        audioMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(wordHighlight, selectedItems[0]!.data);
  });

  test('on highlight change does not close menus', async () => {
    createAudioMenu();
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    audioMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE,
        {detail: {data: audioBrowserProxy.getWordHighlighting()}}));
    await microtasksFinished();

    // Close onClick is false in Improved UI submenus.
    assertEquals(0, closeAllMenusCount);
  });

  test('has phrase highlighting option if flag enabled', () => {
    audioBrowserProxy.isPhraseHighlightingEnabledFlag = true;
    createAudioMenu();
    const highlightGroup = audioMenu.$.menu.menuGroups[1]!;
    const titles = highlightGroup.items.map(item => item.title);
    assertEquals(5, titles.length);
    assertTrue(titles.includes('Phrase'));
  });

  test('does not have phrase highlighting option if flag disabled', () => {
    audioBrowserProxy.isPhraseHighlightingEnabledFlag = false;
    createAudioMenu();
    const highlightGroup = audioMenu.$.menu.menuGroups[1]!;
    const titles = highlightGroup.items.map(item => item.title);
    assertEquals(4, titles.length);
    assertFalse(titles.includes('Phrase'));
  });

  test('restores saved highlight option', async () => {
    createAudioMenu();
    const granularity = audioBrowserProxy.getSentenceHighlighting();
    const startingSelected =
        audioMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertNotEquals(granularity, startingSelected?.data);

    audioMenu.highlightGranularity = granularity;
    await microtasksFinished();

    const newSelected =
        audioMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(granularity, newSelected?.data);
  });

  test('does nothing if saved highlight is the same', async () => {
    createAudioMenu();
    const startingSelected =
        audioMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);

    audioMenu.highlightGranularity = 0;
    await microtasksFinished();

    const newSelected =
        audioMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(startingSelected?.data, newSelected?.data);
  });

  test('can be closed programmatically', () => {
    createAudioMenu();
    stubAnimationFrame();
    audioMenu.open(document.body);
    assertTrue(audioMenu.$.menu.$.lazyMenu.get().open);
    audioMenu.close();
    assertFalse(audioMenu.$.menu.$.lazyMenu.get().open);
  });

  test('opens and closes accent menu', async () => {
    createAudioMenu();
    assertFalse(!!audioMenu.shadowRoot.querySelector('accent-menu'));

    audioMenu.$.menu.dispatchEvent(new CustomEvent('open-accent-menu'));
    await microtasksFinished();

    const accentMenu = audioMenu.shadowRoot.querySelector('accent-menu');
    assertTrue(!!accentMenu);

    accentMenu.dispatchEvent(new CustomEvent('close'));
    await microtasksFinished();

    assertFalse(!!audioMenu.shadowRoot.querySelector('accent-menu'));
  });

  test(
      'opens and closes voice selection dialog from audio menu action item',
      async () => {
        createAudioMenu();
        assertFalse(
            !!audioMenu.shadowRoot.querySelector('voice-selection-dialog'));

        const whenOpenFired =
            eventToPromise(ToolbarEvent.VOICE_MENU_OPEN, audioMenu);
        audioMenu.$.menu.dispatchEvent(
            new CustomEvent('open-voice-selection-dialog'));
        await whenOpenFired;
        await microtasksFinished();

        const dialog =
            audioMenu.shadowRoot.querySelector<VoiceSelectionDialogElement>(
                'voice-selection-dialog');
        assertTrue(!!dialog);

        const whenCloseFired =
            eventToPromise(ToolbarEvent.VOICE_MENU_CLOSE, audioMenu);
        dialog.dispatchEvent(new CustomEvent('close'));
        await whenCloseFired;
        await microtasksFinished();

        assertFalse(
            !!audioMenu.shadowRoot.querySelector('voice-selection-dialog'));
      });

  test(
      'voice item title uses voiceSelectionLabel when selectedVoice is null',
      async () => {
        createAudioMenu();
        await microtasksFinished();

        const voiceGroup = audioMenu.$.menu.menuGroups[0]!;
        assertEquals(
            loadTimeData.getString('voiceSelectionLabel'),
            voiceGroup.items[0]!.title);
      });

  test('voice item title updates when selectedVoice changes', async () => {
    createAudioMenu();
    const voice = createSpeechSynthesisVoice(
        {name: 'Google US English (Natural)', lang: 'en-US'});
    audioMenu.selectedVoice = voice;
    await microtasksFinished();

    const voiceGroup = audioMenu.$.menu.menuGroups[0]!;
    assertEquals(voice.name, voiceGroup.items[0]!.title);
  });

  test('voice item title uses the nature name for a mapped voice', async () => {
    createAudioMenu();
    // Define createSpeechSynthesisVoice with a numbered voice title so it can
    // be used as a key in getVoiceNatureNaming.
    const mappedVoices = [
      createSpeechSynthesisVoice(
          {name: 'Google US English 1 (Natural)', lang: 'en-US'}),
      createSpeechSynthesisVoice({
        name: 'Google español de Estados Unidos 1 (Natural)',
        lang: 'es-US',
      }),
    ];

    for (const voice of mappedVoices) {
      audioMenu.selectedVoice = voice;
      await microtasksFinished();

      const voiceGroup = audioMenu.$.menu.menuGroups[0]!;

      assertEquals(
          getVoiceNatureNaming(voice.name)!.natureName,
          voiceGroup.items[0]!.title);
    }
  });

  // <if expr="not is_chromeos">
  test('voice item title uses system label for non-Google voice', async () => {
    createAudioMenu();
    const systemVoice =
        createSpeechSynthesisVoice({name: 'David', lang: 'en-US'});
    audioMenu.selectedVoice = systemVoice;
    await microtasksFinished();

    const voiceGroup = audioMenu.$.menu.menuGroups[0]!;
    assertEquals(
        loadTimeData.getString('systemVoiceLabel'), voiceGroup.items[0]!.title);
  });
  // </if>

  test(
      'voice group contains voice and accent items with correct event names',
      () => {
        createAudioMenu();
        assertEquals(2, audioMenu.$.menu.menuGroups.length);

        const voiceGroup = audioMenu.$.menu.menuGroups[0]!;
        assertEquals(2, voiceGroup.items.length);

        const voiceItem = voiceGroup.items[0]!;
        assertEquals('open-voice-selection-dialog', voiceItem.eventName);
        const expectedVoiceIcon =
            loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'read-anything:voice-selection' :
            'read-anything:voice-selection-old';
        assertEquals(expectedVoiceIcon, voiceItem.icon);

        const accentItem = voiceGroup.items[1]!;
        assertEquals('open-accent-menu', accentItem.eventName);
        assertEquals('read-anything:translate', accentItem.icon);
        assertEquals(
            loadTimeData.getString('accentMenuLabel'), accentItem.title);
      });

  test('voice and accent items are announced as opening a dialog', async () => {
    createAudioMenu();
    await microtasksFinished();

    const buttons = getItemsInMenu(audioMenu.$.menu.$.lazyMenu);
    // The voice and accent items are the first two items in the menu.
    assertTrue(buttons.length >= 2);
    assertEquals('dialog', buttons[0]!.getAttribute('aria-haspopup'));
    assertEquals('dialog', buttons[1]!.getAttribute('aria-haspopup'));

    // Highlight options act immediately, so they have no popup.
    assertFalse(buttons[2]!.hasAttribute('aria-haspopup'));
  });

  test('voice and accent items show a trailing ellipsis', async () => {
    createAudioMenu();
    await microtasksFinished();

    const buttons = getItemsInMenu(audioMenu.$.menu.$.lazyMenu);
    assertTrue(buttons.length >= 3);

    const voiceTitle = loadTimeData.getString('voiceSelectionLabel');
    assertEquals(
        loadTimeData.getStringF('menuItemOpensDialogLabel', voiceTitle),
        buttons[0]!.textContent.trim());
    assertEquals(voiceTitle, buttons[0]!.getAttribute('aria-label'));
    assertEquals(voiceTitle, buttons[0]!.getAttribute('title'));

    const accentTitle = loadTimeData.getString('accentMenuLabel');
    assertEquals(
        loadTimeData.getStringF('menuItemOpensDialogLabel', accentTitle),
        buttons[1]!.textContent.trim());
    assertEquals(accentTitle, buttons[1]!.getAttribute('aria-label'));
    assertEquals(accentTitle, buttons[1]!.getAttribute('title'));

    // Highlight options act immediately, so they have no ellipsis.
    const highlightTitle = audioMenu.$.menu.menuGroups[1]!.items[0]!.title;
    assertEquals(highlightTitle, buttons[2]!.textContent.trim());
  });

  test('voice item ellipsis follows selectedVoice', async () => {
    createAudioMenu();
    await microtasksFinished();
    // Stamp the lazy menu before the voice changes so the re-render path is
    // exercised, not just the initial render.
    getItemsInMenu(audioMenu.$.menu.$.lazyMenu);

    const voice = createSpeechSynthesisVoice(
        {name: 'Google US English (Natural)', lang: 'en-US'});
    audioMenu.selectedVoice = voice;
    await microtasksFinished();

    const voiceButton = getItemsInMenu(audioMenu.$.menu.$.lazyMenu)[0]!;
    assertEquals(
        loadTimeData.getStringF('menuItemOpensDialogLabel', voice.name),
        voiceButton.textContent.trim());
    assertEquals(voice.name, voiceButton.getAttribute('aria-label'));
    assertEquals(voice.name, voiceButton.getAttribute('title'));
  });
});
