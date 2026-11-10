// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/memory_banks_palette/memory_banks_palette.js';

import {browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {MemoryBanksPaletteElement} from 'chrome://context-hub/memory_banks_palette/memory_banks_palette.js';
import type {MemoryBanksPaletteSettingsElement} from 'chrome://context-hub/memory_banks_palette/memory_banks_palette_settings.js';
import type {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('MemoryBanksPalette', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let palette: MemoryBanksPaletteElement;

  async function createPalette() {
    palette = document.createElement('memory-banks-palette');
    document.body.appendChild(palette);
    await microtasksFinished();
  }

  function query<T extends HTMLElement = HTMLElement>(selector: string): T|
      null {
    return palette.shadowRoot.querySelector<T>(selector);
  }

  function getSettings(): MemoryBanksPaletteSettingsElement {
    return palette.$.settings;
  }

  function assertPaletteBodyHidden(hidden: boolean) {
    for (const selector
             of ['.search-container', '.filter-bar', '.results-list']) {
      assertEquals(hidden, query(selector)!.hidden, selector);
    }
  }

  async function openSettings() {
    palette.$.settingsButton.click();
    await microtasksFinished();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    handler.setResultFor(
        'getAllMemoryBankEntries', Promise.resolve({entries: []}));
    handler.setResultFor(
        'getAllMemoryBankCollections', Promise.resolve({collections: []}));
    handler.setResultFor('getAllMemoryBankTags', Promise.resolve({tags: []}));
    handler.setResultFor(
        'getBrowseMemoryBanksShortcut',
        Promise.resolve({displayText: 'Ctrl+Shift+K'}));
    handler.setResultFor(
        'getSuggestedBrowseMemoryBanksShortcut',
        Promise.resolve({shortcut: 'Alt+Shift+M', displayText: 'Alt+Shift+M'}));
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
  });

  test('LoadsShortcutIntoSettings', async () => {
    await createPalette();
    await handler.whenCalled('getBrowseMemoryBanksShortcut');
    await handler.whenCalled('getSuggestedBrowseMemoryBanksShortcut');
    await microtasksFinished();

    const settings = getSettings();
    assertTrue(settings.hidden);
    assertEquals('Ctrl+Shift+K', settings.currentShortcutDisplayText);
  });

  test('SwitchesBetweenPaletteAndSettings', async () => {
    await createPalette();

    await openSettings();
    assertFalse(getSettings().hidden);
    assertPaletteBodyHidden(true);
    assertTrue(palette.$.settingsButton.hidden);
    // Focus moves to the settings panel's back button.
    assertEquals(
        getSettings().$.backButton, getSettings().shadowRoot.activeElement);

    getSettings().$.backButton.click();
    await microtasksFinished();
    assertTrue(getSettings().hidden);
    assertPaletteBodyHidden(false);
    assertFalse(palette.$.settingsButton.hidden);
    assertEquals(palette.$.settingsButton, palette.shadowRoot.activeElement);
  });

  test('KeepsSearchWhenReturningFromSettings', async () => {
    await createPalette();
    const searchField = query<CrSearchFieldElement>('cr-search-field')!;
    searchField.setValue('travel');
    await microtasksFinished();

    await openSettings();
    getSettings().$.backButton.click();
    await microtasksFinished();

    assertEquals(searchField, query('cr-search-field'));
    assertEquals('travel', searchField.getValue());
  });

  test('UpdatesShortcutWhenChangedInSettings', async () => {
    await createPalette();
    await openSettings();

    getSettings().dispatchEvent(
        new CustomEvent('shortcut-changed', {detail: 'Alt+Shift+M'}));
    await microtasksFinished();

    assertEquals('Alt+Shift+M', getSettings().currentShortcutDisplayText);
  });
});
