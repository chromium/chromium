// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac} from 'chrome://resources/js/platform.js';
import {$, getRequiredElement, isUndoKeyboardEvent, quoteString as quoteStringJs, quoteString} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {Modifier} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('UtilTest', function() {
  test('get elements', function() {
    const element = document.createElement('div');
    element.id = 'foo';
    document.body.appendChild(element);
    assertEquals(element, $('foo'));
    assertEquals(element, getRequiredElement('foo'));

    // $ should not throw if the element does not exist.
    assertEquals(null, $('bar'));

    // getRequiredElement should throw.
    assertThrows(() => getRequiredElement('bar'));
  });

  test('quote string', function() {
    // Basic cases.
    assertEquals('\"test\"', quoteString('"test"'));
    assertEquals('\\!\\?', quoteString('!?'));
    assertEquals(
        '\\(\\._\\.\\) \\( \\:l \\) \\(\\.-\\.\\)',
        quoteString('(._.) ( :l ) (.-.)'));

    // Using the output as a regex.
    let re = new RegExp(quoteString('"hello"'), 'gim');
    let match = re.exec('She said "Hello" loudly');
    assertTrue(!!match);
    assertEquals(9, match.index);

    re = new RegExp(quoteString('Hello, .*'), 'gim');
    match = re.exec('Hello, world');
    assertEquals(null, match);

    // JS version
    // Basic cases.
    assertEquals('\"test\"', quoteStringJs('"test"'));
    assertEquals('\\!\\?', quoteStringJs('!?'));
    assertEquals(
        '\\(\\._\\.\\) \\( \\:l \\) \\(\\.-\\.\\)',
        quoteStringJs('(._.) ( :l ) (.-.)'));

    // Using the output as a regex.
    re = new RegExp(quoteStringJs('"hello"'), 'gim');
    match = re.exec('She said "Hello" loudly');
    assertTrue(!!match);
    assertEquals(9, match.index);

    re = new RegExp(quoteStringJs('Hello, .*'), 'gim');
    match = re.exec('Hello, world');
    assertEquals(null, match);
  });

  test('Ctrl+Z', async function() {
    const eventPromise = eventToPromise('keydown', document.body);
    keyDownOn(document.body, 0, isMac ? 'meta' : 'ctrl', 'z');
    const event = await eventPromise;
    assertTrue(isUndoKeyboardEvent(event));
  });

  test('Ctrl+Not_Z', async function() {
    for (let i = 32; i < 127; i++) {
      const chr = String.fromCharCode(i);
      if (chr.toLowerCase() === 'z') {
        continue;
      }
      const eventPromise = eventToPromise('keydown', document.body);
      keyDownOn(document.body, 0, isMac ? 'meta' : 'ctrl', chr);
      const event = await eventPromise;
      assertFalse(isUndoKeyboardEvent(event));
      assertFalse(event.defaultPrevented);
    }
  });

  test('ModifierCombination+Z', async function() {
    const ctrlModifier = isMac ? 'meta' : 'ctrl';
    const modifierCombinations: Modifier[][] = [
      ['shift'],
      ['shift', 'alt'],
      ['shift', ctrlModifier],
      ['shift', ctrlModifier, 'alt'],
      ['alt'],
      ['alt', ctrlModifier],
    ];
    for (const modifierCombination of modifierCombinations) {
      const eventPromise = eventToPromise('keydown', document.body);
      keyDownOn(document.body, 0, modifierCombination, 'z');
      const event = await eventPromise;
      assertFalse(isUndoKeyboardEvent(event));
      assertFalse(event.defaultPrevented);
    }
  });
});
