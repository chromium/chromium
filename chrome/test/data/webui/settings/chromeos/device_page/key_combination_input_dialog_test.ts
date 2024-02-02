// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {KeyCombinationInputDialogElement} from 'chrome://os-settings/lazy_load.js';
import {fakeMice, KeyEvent, Vkey} from 'chrome://os-settings/os_settings.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<key-combination-input-dialog>', () => {
  let dialog: KeyCombinationInputDialogElement;
  let buttonRemappingChangedEventCount: number = 0;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    if (!dialog) {
      return;
    }
    buttonRemappingChangedEventCount = 0;
    dialog.remove();
  });

  function initializeDialog() {
    dialog = document.createElement(KeyCombinationInputDialogElement.is);
    dialog.set('buttonRemappingList', fakeMice[0]!.settings!.buttonRemappings);
    dialog.set('remappingIndex', 2);
    dialog.addEventListener('button-remapping-changed', function() {
      buttonRemappingChangedEventCount++;
    });
    document.body.appendChild(dialog);
    return flushTasks();
  }

  test('Initialize key combination dialog', async () => {
    await initializeDialog();
    assertTrue(
        !!dialog.shadowRoot!.querySelector('#keyCombinationInputDialog'));
    const saveButton = dialog.shadowRoot!.querySelector('#saveButton');
    assertTrue(!!saveButton);
    const cancelButton = dialog.shadowRoot!.querySelector('#cancelButton');
    assertTrue(!!cancelButton);
  });

  test('Clicking save button will fire event', async () => {
    await initializeDialog();
    assertEquals(buttonRemappingChangedEventCount, 0);
    const saveButton: CrButtonElement|null =
        dialog.shadowRoot!.querySelector('#saveButton');
    assertTrue(!!saveButton);
    dialog.set('inputKeyEvent', {
      vkey: Vkey.kKeyK,
      domCode: 1,
      domKey: 1,
      modifiers: 10,
      keyDisplay: 'K',
    });
    saveButton.click();
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 1);
  });

  test('Dialog listens for shortcut-input-event event', async () => {
    await initializeDialog();

    const updatedKeyEvent: KeyEvent = {
      vkey: Vkey.kKeyT,
      domCode: 1,
      domKey: 1,
      modifiers: 10,
      keyDisplay: 't',
    };

    // Verify that the dialog's inputKeyEvent is empty and not equals to
    // the updatedKeyEvent.
    assertTrue(!dialog.get('inputKeyEvent'));
    dialog.dispatchEvent(new CustomEvent('shortcut-input-event', {
      bubbles: true,
      composed: true,
      detail: {
        keyEvent: updatedKeyEvent,
      },
    }));

    // Verify that the dialog's inputKeyEvent has changed to updatedKeyEvent.
    await flushTasks();
    assertDeepEquals(dialog.get('inputKeyEvent'), updatedKeyEvent);
  });
});
