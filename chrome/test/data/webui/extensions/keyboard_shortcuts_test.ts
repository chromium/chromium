// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-keyboard-shortcuts. */

import 'chrome://extensions/extensions.js';

import type {ExtensionsKeyboardShortcutsElement} from 'chrome://extensions/extensions.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionShortcutTest', function() {
  let keyboardShortcuts: ExtensionsKeyboardShortcutsElement;
  let noCommands: chrome.developerPrivate.ExtensionInfo;
  let oneCommand: chrome.developerPrivate.ExtensionInfo;
  let twoCommands: chrome.developerPrivate.ExtensionInfo;
  let testDelegate: TestService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    keyboardShortcuts = document.createElement('extensions-keyboard-shortcuts');
    testDelegate = new TestService();
    keyboardShortcuts.delegate = testDelegate;

    noCommands = createExtensionInfo({id: 'a'.repeat(32)});
    oneCommand = createExtensionInfo({
      id: 'b'.repeat(32),
      commands: [{
        description: 'Description',
        keybinding: 'Ctrl+W',
        name: 'bCommand',
        isActive: true,
        scope: chrome.developerPrivate.CommandScope.CHROME,
        isExtensionAction: true,
      }],
    });
    twoCommands = createExtensionInfo({
      id: 'c'.repeat(32),
      commands: [
        {
          description: 'Another Description',
          keybinding: 'Alt+F4',
          name: 'cCommand',
          isActive: true,
          scope: chrome.developerPrivate.CommandScope.GLOBAL,
          isExtensionAction: false,
        },
        {
          description: 'Yet Another Description',
          keybinding: '',
          name: 'cCommand2',
          isActive: false,
          scope: chrome.developerPrivate.CommandScope.CHROME,
          isExtensionAction: false,
        },
      ],
    });

    keyboardShortcuts.items = [noCommands, oneCommand, twoCommands];

    document.body.appendChild(keyboardShortcuts);
  });

  test('Layout', function() {
    function isVisibleOnCard(e: HTMLElement, s: string): boolean {
      // We check the light DOM in the card because it's a regular old div,
      // rather than a fancy-schmancy custom element.
      return isChildVisible(e, s, true);
    }
    const cards = keyboardShortcuts.shadowRoot.querySelector('#container')!
                      .querySelectorAll('.shortcut-card');
    assertEquals(2, cards.length);

    const card1 = cards[0]!;
    assertEquals(
        oneCommand.name,
        card1.querySelector<HTMLElement>('.card-title span')!.textContent);
    let commands = card1.querySelectorAll<HTMLElement>('.command-entry');
    assertEquals(1, commands.length);
    assertTrue(isVisibleOnCard(commands[0]!, '.command-name'));
    assertTrue(isVisibleOnCard(commands[0]!, 'select.md-select'));

    const card2 = cards[1]!;
    commands = card2.querySelectorAll('.command-entry');
    assertEquals(2, commands.length);
  });

  test('ScopeChange', async function() {
    const selectElement = keyboardShortcuts.shadowRoot.querySelector('select');
    assertTrue(!!selectElement);
    selectElement.value = 'GLOBAL';
    selectElement.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const params = await testDelegate.whenCalled('updateExtensionCommandScope');
    assertEquals(oneCommand.id, params[0]);
    assertEquals(oneCommand.commands[0]!.name, params[1]);
    await microtasksFinished();
    assertEquals(selectElement.value, params[2]);
  });


  test('UpdateShortcut', async function() {
    const shortcutInput =
        keyboardShortcuts.shadowRoot.querySelector('cr-shortcut-input');
    assertTrue(!!shortcutInput);
    const field = shortcutInput.$.input;
    assertEquals('Ctrl + W', field.value);

    // Click the edit button. Capture should start.
    shortcutInput.$.edit.click();
    let arg = await testDelegate.whenCalled('setShortcutHandlingSuspended');

    assertTrue(arg);
    testDelegate.reset();
    await microtasksFinished();
    assertEquals('', field.value);

    // Press character.
    keyDownOn(field, 65, []);
    await microtasksFinished();
    assertEquals('', field.value);
    assertTrue(field.errorMessage.startsWith('Include'));
    // Add shift to character.
    keyDownOn(field, 65, ['shift']);
    await microtasksFinished();
    assertEquals('', field.value);
    assertTrue(field.errorMessage.startsWith('Include'));
    // Press ctrl.
    keyDownOn(field, 17, ['ctrl']);
    await microtasksFinished();
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);
    // Add shift.
    keyDownOn(field, 16, ['ctrl', 'shift']);
    await microtasksFinished();
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);
    // Remove shift.
    keyUpOn(field, 16, ['ctrl']);
    await microtasksFinished();
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);
    // Add alt (ctrl + alt is invalid).
    keyDownOn(field, 18, ['ctrl', 'alt']);
    await microtasksFinished();
    assertEquals('', field.value);
    // Remove alt.
    keyUpOn(field, 18, ['ctrl']);
    await microtasksFinished();
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);

    // Add 'A'. Once a valid shortcut is typed (like Ctrl + A), it is
    // committed.
    keyDownOn(field, 65, ['ctrl']);
    arg = await testDelegate.whenCalled('updateExtensionCommandKeybinding');
    testDelegate.reset();

    assertDeepEquals(
        [oneCommand.id, oneCommand.commands[0]!.name, 'Ctrl+A'], arg);
    await microtasksFinished();
    assertEquals('Ctrl + A', field.value);
    assertEquals('Ctrl+A', shortcutInput.shortcut);

    // Test clearing the shortcut.
    shortcutInput.$.edit.click();
    assertEquals(shortcutInput.$.input, shortcutInput.shadowRoot.activeElement);
    arg = await testDelegate.whenCalled('updateExtensionCommandKeybinding');
    await microtasksFinished();

    field.blur();
    testDelegate.reset();
    assertDeepEquals([oneCommand.id, oneCommand.commands[0]!.name, ''], arg);
    assertEquals('', shortcutInput.shortcut);

    // The click event causes the input element to lose focus on mouse down
    // but regains focus on mouse up after triggering the edit button on mouse
    // up. This should ultimately result in shortcuts being suspended.
    shortcutInput.$.edit.click();
    await testDelegate.whenCalled('setShortcutHandlingSuspended');
    await microtasksFinished();
    const shortcutSuspendedArgs =
        testDelegate.getArgs('setShortcutHandlingSuspended');
    assertEquals(2, testDelegate.getCallCount('setShortcutHandlingSuspended'));
    assertFalse(shortcutSuspendedArgs[0]);
    assertTrue(shortcutSuspendedArgs[1]);
    testDelegate.reset();

    // Test ending capture using the escape key.
    shortcutInput.$.edit.click();
    keyDownOn(field, 27);  // Escape key.
    arg = await testDelegate.whenCalled('setShortcutHandlingSuspended');
    assertFalse(arg);
  });
});
