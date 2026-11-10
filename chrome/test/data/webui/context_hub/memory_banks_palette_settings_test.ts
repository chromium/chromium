// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/memory_banks_palette/memory_banks_palette_settings.js';

import {browserProxyFactory, MemoryBanksShortcutStatus, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {MemoryBanksPaletteSettingsElement} from 'chrome://context-hub/memory_banks_palette/memory_banks_palette_settings.js';
import {keyboardEventToShortcut} from 'chrome://context-hub/memory_banks_palette/memory_banks_palette_settings.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

const KEY_K = 75;

/** The platform's primary shortcut modifier: Command on macOS, else Ctrl. */
function primaryModifier(): KeyboardEventInit {
  return isMac ? {metaKey: true} : {ctrlKey: true};
}

function expectedShortcutString(): string {
  return isMac ? 'Command+Shift+K' : 'Ctrl+Shift+K';
}

function createKeyEvent(type: string, init: KeyboardEventInit): KeyboardEvent {
  return new KeyboardEvent(
      type, {bubbles: true, cancelable: true, composed: true, ...init});
}

/** A press of the K key with the given modifiers. */
function keyK(modifiers: KeyboardEventInit = {}): KeyboardEvent {
  return createKeyEvent('keydown', {key: 'k', keyCode: KEY_K, ...modifiers});
}

suite('MemoryBanksPaletteSettings', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let settings: MemoryBanksPaletteSettingsElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    settings = document.createElement('memory-banks-palette-settings');
    document.body.appendChild(settings);
    await microtasksFinished();
  });

  function getField(): HTMLElement {
    return settings.$.shortcutField;
  }

  function getById(id: string): HTMLElement|null {
    return settings.shadowRoot.querySelector<HTMLElement>(`#${id}`);
  }

  async function startRecording() {
    getById('editButton')!.click();
    await microtasksFinished();
    assertTrue(settings.isRecording);
  }

  function setSaveResults(
      status: MemoryBanksShortcutStatus, displayText: string = '') {
    handler.setResultFor(
        'setBrowseMemoryBanksShortcut', Promise.resolve({status}));
    handler.setResultFor(
        'getBrowseMemoryBanksShortcut', Promise.resolve({displayText}));
  }

  test('ConvertsKeyEventsToShortcutStrings', () => {
    assertEquals(
        expectedShortcutString(),
        keyboardEventToShortcut(keyK({shiftKey: true, ...primaryModifier()})));
    assertEquals(
        'Alt+Shift+K',
        keyboardEventToShortcut(keyK({altKey: true, shiftKey: true})));
    assertEquals(
        'Alt+7',
        keyboardEventToShortcut(createKeyEvent(
            'keydown', {key: '7', keyCode: '7'.charCodeAt(0), altKey: true})));
    // Shift by itself is not enough.
    assertEquals('', keyboardEventToShortcut(keyK({shiftKey: true})));
    // Only letters and numbers are supported.
    assertEquals(
        '',
        keyboardEventToShortcut(createKeyEvent(
            'keydown', {key: ' ', keyCode: 32, ...primaryModifier()})));
    // The Windows key and the ChromeOS Search key are reserved.
    if (!isMac) {
      assertEquals('', keyboardEventToShortcut(keyK({metaKey: true})));
    }
  });

  test('ShowsNotSetByDefault', () => {
    assertEquals('Not set', getField().textContent.trim());
    assertEquals('Set shortcut', getById('editButton')!.textContent.trim());
    assertNull(getById('removeButton'));
  });

  test('ShowsConfiguredShortcut', async () => {
    settings.currentShortcutDisplayText = 'Ctrl+Shift+K';
    await microtasksFinished();
    assertEquals('Ctrl+Shift+K', getField().textContent.trim());
    assertEquals('Change', getById('editButton')!.textContent.trim());
    assertTrue(!!getById('removeButton'));
  });

  test('ClickingFieldStartsRecording', async () => {
    getField().click();
    await microtasksFinished();
    assertTrue(settings.isRecording);
    assertEquals(getField(), settings.shadowRoot.activeElement);
  });

  test('RecordsAndSavesShortcut', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kValid, 'Ctrl+Shift+K');
    await startRecording();
    assertEquals(getField(), settings.shadowRoot.activeElement);

    // Holding modifiers shows a live preview without saving.
    getField().dispatchEvent(createKeyEvent(
        'keydown', {key: 'Shift', shiftKey: true, ...primaryModifier()}));
    await microtasksFinished();
    assertEquals(isMac ? '⇧⌘' : 'Ctrl+Shift+', getField().textContent.trim());
    assertEquals(0, handler.getCallCount('setBrowseMemoryBanksShortcut'));

    const changed =
        eventToPromise<CustomEvent<string>>('shortcut-changed', settings);
    getField().dispatchEvent(keyK({shiftKey: true, ...primaryModifier()}));

    assertEquals(
        expectedShortcutString(),
        await handler.whenCalled('setBrowseMemoryBanksShortcut'));
    assertEquals('Ctrl+Shift+K', (await changed).detail);
    await microtasksFinished();
    assertFalse(settings.isRecording);
    assertEquals('Ctrl+Shift+K', settings.currentShortcutDisplayText);
    assertNull(getById('error'));
    // Focus moves to the button that replaced the shortcut field.
    assertEquals(getById('editButton'), settings.shadowRoot.activeElement);
  });

  test('ShowsErrorForConflictingShortcut', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kConflictsWithBrowserShortcut);
    await startRecording();

    getField().dispatchEvent(keyK({shiftKey: true, ...primaryModifier()}));
    await handler.whenCalled('setBrowseMemoryBanksShortcut');
    await microtasksFinished();

    assertTrue(settings.isRecording);
    assertEquals('', settings.currentShortcutDisplayText);
    assertTrue(!!getById('error'));
  });

  test('ShowsErrorForInvalidShortcut', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kInvalid);
    await startRecording();

    getField().dispatchEvent(keyK({shiftKey: true, ...primaryModifier()}));
    await handler.whenCalled('setBrowseMemoryBanksShortcut');
    await microtasksFinished();

    assertTrue(settings.isRecording);
    assertEquals('', settings.currentShortcutDisplayText);
    assertTrue(!!getById('error'));
  });

  test('ShowsErrorForDisallowedModifiers', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kDisallowedModifiers);
    await startRecording();

    getField().dispatchEvent(keyK({shiftKey: true, ...primaryModifier()}));
    await handler.whenCalled('setBrowseMemoryBanksShortcut');
    await microtasksFinished();

    assertTrue(settings.isRecording);
    assertEquals('', settings.currentShortcutDisplayText);
    assertTrue(!!getById('error'));
  });

  test('ShowsErrorWithoutModifier', async () => {
    await startRecording();

    getField().dispatchEvent(keyK({shiftKey: true}));
    await microtasksFinished();

    assertEquals(0, handler.getCallCount('setBrowseMemoryBanksShortcut'));
    assertTrue(!!getById('error'));
  });

  test('ShowsErrorForModifiedTab', async () => {
    await startRecording();

    getField().dispatchEvent(
        createKeyEvent('keydown', {key: 'Tab', keyCode: 9, ctrlKey: true}));
    await microtasksFinished();

    assertEquals(0, handler.getCallCount('setBrowseMemoryBanksShortcut'));
    assertTrue(settings.isRecording);
    assertTrue(!!getById('error'));
  });

  test('ShowsSpecificErrorsForUnsupportedModifiers', async () => {
    await startRecording();

    // The Windows key and the ChromeOS Search key are reserved; macOS allows
    // up to three modifier keys.
    getField().dispatchEvent(
        isMac ?
            keyK({metaKey: true, ctrlKey: true, altKey: true, shiftKey: true}) :
            keyK({metaKey: true, ctrlKey: true}));
    await microtasksFinished();

    assertEquals(0, handler.getCallCount('setBrowseMemoryBanksShortcut'));
    assertEquals(
        isMac ? 'Use up to three modifier keys.' :
                'The Windows and Search keys are reserved. Use Ctrl or Alt ' +
                'instead.',
        getById('error')!.textContent.trim());
  });

  test('IgnoresRepeatedAndComposingKeys', async () => {
    await startRecording();

    getField().dispatchEvent(
        keyK({repeat: true, shiftKey: true, ...primaryModifier()}));
    getField().dispatchEvent(
        keyK({isComposing: true, shiftKey: true, ...primaryModifier()}));
    await microtasksFinished();

    assertEquals(0, handler.getCallCount('setBrowseMemoryBanksShortcut'));
    assertTrue(settings.isRecording);
  });

  test('NewModifierClearsError', async () => {
    await startRecording();
    getField().dispatchEvent(keyK({shiftKey: true}));
    await microtasksFinished();
    assertTrue(!!getById('error'));

    // AltGraph counts as a modifier, so it starts a new combination.
    getField().dispatchEvent(
        createKeyEvent('keydown', {key: 'AltGraph', altKey: true}));
    await microtasksFinished();

    assertNull(getById('error'));
    assertFalse(getField().classList.contains('invalid'));
  });

  test('EscapeCancelsRecording', async () => {
    await startRecording();
    // Shows an error first, which cancelling should clear.
    getField().dispatchEvent(keyK({shiftKey: true}));
    await microtasksFinished();
    assertTrue(!!getById('error'));

    getField().dispatchEvent(createKeyEvent('keydown', {key: 'Escape'}));
    await microtasksFinished();

    assertFalse(settings.isRecording);
    assertEquals(0, handler.getCallCount('setBrowseMemoryBanksShortcut'));
    assertNull(getById('error'));
    assertFalse(getField().classList.contains('invalid'));
    assertEquals(getById('editButton'), settings.shadowRoot.activeElement);
  });

  test('BlurCancelsRecording', async () => {
    await settings.startRecording();

    getField().dispatchEvent(new FocusEvent('blur'));
    await microtasksFinished();

    assertFalse(settings.isRecording);
    assertTrue(!!getById('editButton'));
  });

  test('IgnoresErrorAfterRecordingIsCancelled', async () => {
    let resolveSave: (value: {status: MemoryBanksShortcutStatus}) => void =
        () => {};
    handler.setResultFor(
        'setBrowseMemoryBanksShortcut', new Promise(resolve => {
          resolveSave = resolve;
        }));
    await startRecording();
    getField().dispatchEvent(keyK({shiftKey: true, ...primaryModifier()}));
    await handler.whenCalled('setBrowseMemoryBanksShortcut');

    // Cancel before the browser answers.
    getField().dispatchEvent(createKeyEvent('keydown', {key: 'Escape'}));
    resolveSave(
        {status: MemoryBanksShortcutStatus.kConflictsWithBrowserShortcut});
    await microtasksFinished();

    assertFalse(settings.isRecording);
    assertNull(getById('error'));
  });

  test('RemovesShortcut', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kValid, '');
    settings.currentShortcutDisplayText = 'Ctrl+Shift+K';
    await microtasksFinished();

    const changed =
        eventToPromise<CustomEvent<string>>('shortcut-changed', settings);
    getById('removeButton')!.click();

    assertEquals('', await handler.whenCalled('setBrowseMemoryBanksShortcut'));
    assertEquals('', (await changed).detail);
    await microtasksFinished();
    assertEquals('Not set', getField().textContent.trim());
    assertEquals(getById('editButton'), settings.shadowRoot.activeElement);
  });

  test('ShowsAndAppliesSuggestedShortcut', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kValid, 'Alt+Shift+M');
    settings.suggestedShortcutAccelerator = 'Alt+Shift+M';
    settings.suggestedShortcutDisplayText = 'Alt+Shift+M';
    await microtasksFinished();
    assertTrue(!!getById('suggestion'));

    const changed =
        eventToPromise<CustomEvent<string>>('shortcut-changed', settings);
    getById('useSuggestedButton')!.click();

    assertEquals(
        'Alt+Shift+M',
        await handler.whenCalled('setBrowseMemoryBanksShortcut'));
    assertEquals('Alt+Shift+M', (await changed).detail);
    await microtasksFinished();
    assertEquals('Alt+Shift+M', settings.currentShortcutDisplayText);
    // The suggestion is hidden once a shortcut is set.
    assertNull(getById('suggestion'));
  });

  test('ShowsErrorWhenSuggestedShortcutFails', async () => {
    setSaveResults(MemoryBanksShortcutStatus.kConflictsWithBrowserShortcut);
    settings.suggestedShortcutAccelerator = 'Alt+Shift+M';
    settings.suggestedShortcutDisplayText = 'Alt+Shift+M';
    await microtasksFinished();

    getById('useSuggestedButton')!.click();
    await handler.whenCalled('setBrowseMemoryBanksShortcut');
    await microtasksFinished();

    assertEquals('', settings.currentShortcutDisplayText);
    assertTrue(!!getById('error'));
  });

  test('HidesSuggestionWhileRecordingOrWhenUnavailable', async () => {
    // No suggestion from the browser.
    assertNull(getById('suggestion'));

    // Display text alone isn't enough; there must be a shortcut to apply.
    settings.suggestedShortcutDisplayText = 'Alt+Shift+M';
    await microtasksFinished();
    assertNull(getById('suggestion'));

    settings.suggestedShortcutAccelerator = 'Alt+Shift+M';
    await microtasksFinished();
    assertTrue(!!getById('suggestion'));

    await startRecording();
    assertNull(getById('suggestion'));
  });

  test('BackButtonFiresBackClick', async () => {
    const backClick = eventToPromise('back-click', settings);
    settings.$.backButton.click();
    await backClick;
  });
});
