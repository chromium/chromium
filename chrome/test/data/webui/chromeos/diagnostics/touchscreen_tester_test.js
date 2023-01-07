// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

const SCREEN_MAX_LENGTH = 9999;

export function touchscreenTesterTestSuite() {
  /** @type {?TouchscreenTesterElement} */
  let touchscreenTesterElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    touchscreenTesterElement.remove();
    touchscreenTesterElement = null;
  });


  function initializeTouchscreenTester() {
    assertFalse(!!touchscreenTesterElement);
    touchscreenTesterElement = /** @type {!TouchscreenTesterElement} */ (
        document.createElement('touchscreen-tester'));
    assertTrue(!!touchscreenTesterElement);
    document.body.appendChild(touchscreenTesterElement);

    return flushTasks();
  }

  test('openIntroDialog', async () => {
    await initializeTouchscreenTester();
    const introDialog = touchscreenTesterElement.getDialog('intro-dialog');
    introDialog.showModal();
    await flushTasks();
    assertTrue(introDialog.open);
  });

  test('openCanvasDialog', async () => {
    await initializeTouchscreenTester();
    const introDialog = touchscreenTesterElement.getDialog('intro-dialog');
    introDialog.showModal();
    await flushTasks();
    assertTrue(introDialog.open);

    const getStartedButton = introDialog.querySelector('cr-button');
    getStartedButton.click();
    await flushTasks();
    assertFalse(introDialog.open);

    const canvasDialog = touchscreenTesterElement.getDialog('canvas-dialog');
    assertTrue(canvasDialog.open);

    const canvas = canvasDialog.querySelector('canvas');
    assertEquals(canvas.width, SCREEN_MAX_LENGTH);
    assertEquals(canvas.height, SCREEN_MAX_LENGTH);
  });
}
