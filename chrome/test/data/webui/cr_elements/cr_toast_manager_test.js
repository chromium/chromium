// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {getInstance} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
// #import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-toast-manager', () => {
  let toastManager;

  suiteSetup(() => {
    PolymerTest.clearBody();
    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('getInstance', () => {
    assertEquals(toastManager, cr.toastManager.getInstance());
  });

  test('simple show/hide', function() {
    toastManager.show('test', false);
    assertEquals('test', toastManager.$.content.textContent);
    assertTrue(toastManager.$.button.hidden);

    toastManager.hide();

    toastManager.show('test', true);
    assertFalse(toastManager.$.button.hidden);

    toastManager.hide();
    const whenDone =
        new Promise(resolve => window.requestAnimationFrame(resolve));
    return whenDone.then(() => {
      assertEquals(
          'hidden', window.getComputedStyle(toastManager.$.button).visibility);
    });
  });

  test('undo-click fired when undo button is clicked', async () => {
    toastManager.show('test', true);
    const wait = test_util.eventToPromise('undo-click', toastManager);
    toastManager.$.button.click();
    await wait;
  });

  test('showForStringPieces', () => {
    const pieces = [
      {value: '\'', collapsible: false},
      {value: 'folder', collapsible: true},
      {value: '\' copied', collapsible: false},
    ];
    toastManager.showForStringPieces(pieces, false);
    const elements = toastManager.$.content.querySelectorAll('span');
    assertEquals(3, elements.length);
    assertFalse(elements[0].classList.contains('collapsible'));
    assertTrue(elements[1].classList.contains('collapsible'));
    assertFalse(elements[2].classList.contains('collapsible'));
  });

  test('duration passed through to toast', () => {
    toastManager.duration = 3;
    assertEquals(3, toastManager.$.toast.duration);
  });

  test('undo description and label', () => {
    toastManager.undoDescription = 'description';
    const button = toastManager.$$('#button[aria-label="description"]');
    assertTrue(!!button);
    assertEquals('', button.textContent.trim());
    toastManager.undoLabel = 'undo';
    assertEquals('undo', button.textContent.trim());
  });
});
