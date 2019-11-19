// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-keyboard-shortcuts. */

import {isValidKeyCode, Key, keystrokeToString} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isVisible} from '../test_util.m.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

window.extension_shortcut_tests = {};
extension_shortcut_tests.suiteName = 'ExtensionShortcutTest';
/** @enum {string} */
extension_shortcut_tests.TestNames = {
  IsValidKeyCode: 'isValidKeyCode',
  KeyStrokeToString: 'keystrokeToString',
  Layout: 'Layout',
  ScopeChange: 'ScopeChange',
};

suite(extension_shortcut_tests.suiteName, function() {
  /** @type {KeyboardShortcuts} */
  let keyboardShortcuts;
  /** @type {chrome.developerPrivate.ExtensionInfo} */
  let noCommands;
  /** @type {chrome.developerPrivate.ExtensionInfo} */
  let oneCommand;
  /** @type {chrome.developerPrivate.ExtensionInfo} */
  let twoCommands;

  setup(function() {
    PolymerTest.clearBody();
    keyboardShortcuts = document.createElement('extensions-keyboard-shortcuts');
    keyboardShortcuts.delegate = new TestService();

    noCommands = createExtensionInfo({id: 'a'.repeat(32)});
    oneCommand = createExtensionInfo({
      id: 'b'.repeat(32),
      commands: [{
        description: 'Description',
        keybinding: 'Ctrl+W',
        name: 'bCommand',
        isActive: true,
        scope: 'CHROME',
        isExtensionAction: true,
      }]
    });
    twoCommands = createExtensionInfo({
      id: 'c'.repeat(32),
      commands: [
        {
          description: 'Another Description',
          keybinding: 'Alt+F4',
          name: 'cCommand',
          isActive: true,
          scope: 'GLOBAL',
          isExtensionAction: false,
        },
        {
          description: 'Yet Another Description',
          keybinding: '',
          name: 'cCommand2',
          isActive: false,
          scope: 'CHROME',
          isExtensionAction: false,
        }
      ]
    });

    keyboardShortcuts.set('items', [noCommands, oneCommand, twoCommands]);

    document.body.appendChild(keyboardShortcuts);

    flush();
  });

  test(assert(extension_shortcut_tests.TestNames.Layout), function() {
    const isVisibleOnCard = function(e, s) {
      // We check the light DOM in the card because it's a regular old div,
      // rather than a fancy-schmancy custom element.
      return isVisible(e, s, true);
    };
    const cards =
        keyboardShortcuts.$$('#container').querySelectorAll('.shortcut-card');
    assertEquals(2, cards.length);

    const card1 = cards[0];
    expectEquals(
        oneCommand.name, card1.querySelector('.card-title span').textContent);
    let commands = card1.querySelectorAll('.command-entry');
    assertEquals(1, commands.length);
    expectTrue(isVisibleOnCard(commands[0], '.command-name'));
    expectTrue(isVisibleOnCard(commands[0], 'select.md-select'));

    const card2 = cards[1];
    commands = card2.querySelectorAll('.command-entry');
    assertEquals(2, commands.length);
  });

  test(extension_shortcut_tests.TestNames.IsValidKeyCode, function() {
    expectTrue(isValidKeyCode('A'.charCodeAt(0)));
    expectTrue(isValidKeyCode('F'.charCodeAt(0)));
    expectTrue(isValidKeyCode('Z'.charCodeAt(0)));
    expectTrue(isValidKeyCode('4'.charCodeAt(0)));
    expectTrue(isValidKeyCode(Key.PageUp));
    expectTrue(isValidKeyCode(Key.MediaPlayPause));
    expectTrue(isValidKeyCode(Key.Down));
    expectFalse(isValidKeyCode(16));   // Shift
    expectFalse(isValidKeyCode(17));   // Ctrl
    expectFalse(isValidKeyCode(18));   // Alt
    expectFalse(isValidKeyCode(113));  // F2
    expectFalse(isValidKeyCode(144));  // Num Lock
    expectFalse(isValidKeyCode(43));   // +
    expectFalse(isValidKeyCode(27));   // Escape
  });

  test(extension_shortcut_tests.TestNames.KeyStrokeToString, function() {
    // Creating an event with the KeyboardEvent ctor doesn't work. Fake it.
    const e = {keyCode: 'A'.charCodeAt(0)};
    expectEquals('A', keystrokeToString(e));
    e.ctrlKey = true;
    expectEquals('Ctrl+A', keystrokeToString(e));
    e.shiftKey = true;
    expectEquals('Ctrl+Shift+A', keystrokeToString(e));
  });

  test(extension_shortcut_tests.TestNames.ScopeChange, function() {
    const selectElement = keyboardShortcuts.$$('select');
    selectElement.value = 'GLOBAL';
    selectElement.dispatchEvent(new CustomEvent('change'));
    return keyboardShortcuts.delegate.whenCalled('updateExtensionCommandScope')
        .then(params => {
          assertEquals(oneCommand.id, params[0]);
          assertEquals(oneCommand.commands[0].name, params[1]);
          assertEquals(selectElement.value, params[2]);
        });
  });
});
