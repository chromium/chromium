// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-keyboard-shortcuts. */

import type {ExtensionsKeyboardShortcutsElement} from 'chrome://extensions/extensions.js';
import {isValidKeyCode, Key, keystrokeToString} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    const cards = keyboardShortcuts.shadowRoot!.querySelector('#container')!
                      .querySelectorAll('.shortcut-card');
    assertEquals(2, cards.length);

    const card1 = cards[0]!;
    assertEquals(
        oneCommand.name,
        card1.querySelector<HTMLElement>('.card-title span')!.textContent!);
    let commands = card1.querySelectorAll<HTMLElement>('.command-entry');
    assertEquals(1, commands.length);
    assertTrue(isVisibleOnCard(commands[0]!, '.command-name'));
    assertTrue(isVisibleOnCard(commands[0]!, 'select.md-select'));

    const card2 = cards[1]!;
    commands = card2.querySelectorAll('.command-entry');
    assertEquals(2, commands.length);
  });

  test('IsValidKeyCode', function() {
    assertTrue(isValidKeyCode('A'.charCodeAt(0)));
    assertTrue(isValidKeyCode('F'.charCodeAt(0)));
    assertTrue(isValidKeyCode('Z'.charCodeAt(0)));
    assertTrue(isValidKeyCode('4'.charCodeAt(0)));
    assertTrue(isValidKeyCode(Key.PAGE_UP));
    assertTrue(isValidKeyCode(Key.MEDIA_PLAY_PAUSE));
    assertTrue(isValidKeyCode(Key.DOWN));
    assertFalse(isValidKeyCode(16));   // Shift
    assertFalse(isValidKeyCode(17));   // Ctrl
    assertFalse(isValidKeyCode(18));   // Alt
    assertFalse(isValidKeyCode(113));  // F2
    assertFalse(isValidKeyCode(144));  // Num Lock
    assertFalse(isValidKeyCode(43));   // +
    assertFalse(isValidKeyCode(27));   // Escape
  });

  test('KeyStrokeToString', function() {
    const charCodeA = 'A'.charCodeAt(0);
    let e = new KeyboardEvent('keydown', {keyCode: charCodeA});
    assertEquals('A', keystrokeToString(e));
    e = new KeyboardEvent('keydown', {keyCode: charCodeA, ctrlKey: true});
    assertEquals('Ctrl+A', keystrokeToString(e));
    e = new KeyboardEvent(
        'keydown', {keyCode: charCodeA, ctrlKey: true, shiftKey: true});
    assertEquals('Ctrl+Shift+A', keystrokeToString(e));
  });

  test('ScopeChange', async function() {
    const selectElement = keyboardShortcuts.shadowRoot!.querySelector('select');
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
});
