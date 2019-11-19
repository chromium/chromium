// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-shortcut-input. */

import 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {keyDownOn, keyUpOn, tap} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isVisible} from '../test_util.m.js';

import {TestService} from './test_service.js';

window.extension_shortcut_input_tests = {};
extension_shortcut_input_tests.suiteName = 'ExtensionShortcutInputTest';
/** @enum {string} */
extension_shortcut_input_tests.TestNames = {
  Basic: 'basic',
};

suite(extension_shortcut_input_tests.suiteName, function() {
  /** @type {ExtensionsShortcutInputElement} */
  let input;

  setup(function() {
    PolymerTest.clearBody();
    input = document.createElement('extensions-shortcut-input');
    input.delegate = new TestService();
    input.commandName = 'Command';
    input.item = 'itemid';
    document.body.appendChild(input);
    flush();
  });

  test(assert(extension_shortcut_input_tests.TestNames.Basic), function() {
    const field = input.$['input'];
    assertEquals('', field.value);
    const isClearVisible = isVisible.bind(null, input, '#clear', false);
    expectFalse(isClearVisible());

    // Click the input. Capture should start.
    tap(field);
    return input.delegate.whenCalled('setShortcutHandlingSuspended')
        .then((arg) => {
          assertTrue(arg);
          input.delegate.reset();

          assertEquals('', field.value);
          expectFalse(isClearVisible());

          // Press character.
          keyDownOn(field, 'A', []);
          assertEquals('', field.value);
          expectTrue(field.errorMessage.startsWith('Include'));
          // Add shift to character.
          keyDownOn(field, 'A', ['shift']);
          assertEquals('', field.value);
          expectTrue(field.errorMessage.startsWith('Include'));
          // Press ctrl.
          keyDownOn(field, 17, ['ctrl']);
          assertEquals('Ctrl', field.value);
          assertEquals('Type a letter', field.errorMessage);
          // Add shift.
          keyDownOn(field, 16, ['ctrl', 'shift']);
          assertEquals('Ctrl + Shift', field.value);
          assertEquals('Type a letter', field.errorMessage);
          // Remove shift.
          keyUpOn(field, 16, ['ctrl']);
          assertEquals('Ctrl', field.value);
          assertEquals('Type a letter', field.errorMessage);
          // Add alt (ctrl + alt is invalid).
          keyDownOn(field, 18, ['ctrl', 'alt']);
          assertEquals('Ctrl', field.value);
          // Remove alt.
          keyUpOn(field, 18, ['ctrl']);
          assertEquals('Ctrl', field.value);
          assertEquals('Type a letter', field.errorMessage);

          // Add 'A'. Once a valid shortcut is typed (like Ctrl + A), it is
          // committed.
          keyDownOn(field, 65, ['ctrl']);
          return input.delegate.whenCalled('updateExtensionCommandKeybinding');
        })
        .then((arg) => {
          input.delegate.reset();
          expectDeepEquals(['itemid', 'Command', 'Ctrl+A'], arg);
          assertEquals('Ctrl + A', field.value);
          assertEquals('Ctrl+A', input.shortcut);
          expectTrue(isClearVisible());

          // Test clearing the shortcut.
          tap(input.$['clear']);
          return input.delegate.whenCalled('updateExtensionCommandKeybinding');
        })
        .then((arg) => {
          input.delegate.reset();
          expectDeepEquals(['itemid', 'Command', ''], arg);
          assertEquals('', input.shortcut);
          expectFalse(isClearVisible());

          tap(field);
          return input.delegate.whenCalled('setShortcutHandlingSuspended');
        })
        .then((arg) => {
          input.delegate.reset();
          expectTrue(arg);

          // Test ending capture using the escape key.
          keyDownOn(field, 27);  // Escape key.
          return input.delegate.whenCalled('setShortcutHandlingSuspended');
        })
        .then(expectFalse);
  });
});
