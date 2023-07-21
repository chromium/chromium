// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-shortcut-input. */

import 'chrome://extensions/extensions.js';

import {ExtensionsShortcutInputElement} from 'chrome://extensions/extensions.js';
import {keyDownOn, keyUpOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionShortcutInputTest', function() {
  let input: ExtensionsShortcutInputElement;
  let testService: TestService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    input = document.createElement('extensions-shortcut-input');
    testService = new TestService();
    input.delegate = testService;
    input.command = {
      description: 'Command description',
      keybinding: 'Ctrl+W',
      name: 'Command',
      isActive: true,
      scope: chrome.developerPrivate.CommandScope.CHROME,
      isExtensionAction: true,
    };
    input.item = createExtensionInfo({id: 'itemid'});
    document.body.appendChild(input);
    flush();
  });

  test('Basic', async function() {
    const field = input.$.input;
    assertEquals('', field.value);

    // Click the edit button. Capture should start.
    input.$.edit.click();
    let arg = await testService.whenCalled('setShortcutHandlingSuspended');

    assertTrue(arg);
    testService.reset();
    assertEquals('', field.value);

    // Press character.
    keyDownOn(field, 65, []);
    assertEquals('', field.value);
    assertTrue(field.errorMessage!.startsWith('Include'));
    // Add shift to character.
    keyDownOn(field, 65, ['shift']);
    assertEquals('', field.value);
    assertTrue(field.errorMessage!.startsWith('Include'));
    // Press ctrl.
    keyDownOn(field, 17, ['ctrl']);
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);
    // Add shift.
    keyDownOn(field, 16, ['ctrl', 'shift']);
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);
    // Remove shift.
    keyUpOn(field, 16, ['ctrl']);
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);
    // Add alt (ctrl + alt is invalid).
    keyDownOn(field, 18, ['ctrl', 'alt']);
    assertEquals('', field.value);
    // Remove alt.
    keyUpOn(field, 18, ['ctrl']);
    assertEquals('', field.value);
    assertEquals('Type a letter', field.errorMessage);

    // Add 'A'. Once a valid shortcut is typed (like Ctrl + A), it is
    // committed.
    keyDownOn(field, 65, ['ctrl']);
    arg = await testService.whenCalled('updateExtensionCommandKeybinding');

    testService.reset();
    assertDeepEquals(['itemid', 'Command', 'Ctrl+A'], arg);
    assertEquals('Ctrl + A', field.value);
    assertEquals('Ctrl+A', input.shortcut);

    // Test clearing the shortcut.
    input.$.edit.click();
    assertEquals(input.$.input, input.shadowRoot!.activeElement);
    arg = await testService.whenCalled('updateExtensionCommandKeybinding');

    field.blur();
    testService.reset();
    assertDeepEquals(['itemid', 'Command', ''], arg);
    assertEquals('', input.shortcut);

    input.$.edit.click();
    arg = await testService.whenCalled('setShortcutHandlingSuspended');
    testService.reset();
    assertTrue(arg);

    // Test ending capture using the escape key.
    input.$.edit.click();
    keyDownOn(field, 27);  // Escape key.
    arg = await testService.whenCalled('setShortcutHandlingSuspended');
    assertFalse(arg);
  });
});
