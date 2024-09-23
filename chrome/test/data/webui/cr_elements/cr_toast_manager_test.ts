// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';

import type {CrToastManagerElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-toast-manager', () => {
  let toastManager: CrToastManagerElement;

  suiteSetup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('getToastManager', () => {
    assertEquals(toastManager, getToastManager());
  });

  test('simple show/hide', () => {
    assertFalse(toastManager.isToastOpen);
    toastManager.show('test');
    assertEquals('test', toastManager.$.content.textContent);
    assertTrue(toastManager.isToastOpen);
    toastManager.hide();
    assertFalse(toastManager.isToastOpen);
  });

  test('showForStringPieces', () => {
    const pieces = [
      {value: '\'', collapsible: false},
      {value: 'folder', collapsible: true},
      {value: '\' copied', collapsible: false},
    ];
    toastManager.showForStringPieces(pieces);
    const elements = toastManager.$.content.querySelectorAll('span');
    assertEquals(3, elements.length);
    assertFalse(elements[0]!.classList.contains('collapsible'));
    assertTrue(elements[1]!.classList.contains('collapsible'));
    assertFalse(elements[2]!.classList.contains('collapsible'));
  });

  test('duration passed through to toast', async () => {
    toastManager.duration = 3;
    await microtasksFinished();
    assertEquals(3, toastManager.$.toast.duration);
  });

  test('slot hidden or shown based on arg passed into |show()|', () => {
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    // Check that |hideSlotted| defaults to false.
    toastManager.show('');
    assertFalse(toastManager.slottedHidden);
  });
});
