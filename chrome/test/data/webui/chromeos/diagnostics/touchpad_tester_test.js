// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://diagnostics/strings.m.js';

import {fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {TouchpadTesterElement} from 'chrome://diagnostics/touchpad_tester.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {MockController} from '../mock_controller.m.js';
import {isChildVisible, isVisible} from '../test_util.js';

import {assertElementContainsText} from './diagnostics_test_utils.js';

suite('touchpadTesterTestSuite', function() {
  const titleSlotSelector = '#touchpadTesterDialog div[slot=title]';

  /** @type {?TouchpadTesterElement} */
  let touchpadTesterElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    touchpadTesterElement.remove();
    touchpadTesterElement = null;
  });

  /**
   * Adds tester to page DOM.
   * @return {!Promise}
   */
  function initializeElement() {
    touchpadTesterElement = /** @type {!TouchpadTesterElement} */ (
        document.createElement(TouchpadTesterElement.is));
    assertTrue(!!touchpadTesterElement);
    document.body.appendChild(touchpadTesterElement);

    return flushTasks();
  }

  test('VerifyElementInitalizedCorrectly', async () => {
    await initializeElement();
    assertFalse(isChildVisible(touchpadTesterElement, titleSlotSelector));
  });

  test('VerifyShowAndCloseUpdateDialogOpenState', async () => {
    await initializeElement();
    /**@type {!CrDialogElement}*/
    const dialog = touchpadTesterElement.$.touchpadTesterDialog;
    assertFalse(isChildVisible(touchpadTesterElement, titleSlotSelector));

    // Display tester dialog.
    const touchpad = fakeTouchDevices[0];
    touchpadTesterElement.show(touchpad);
    await flushTasks();

    assertDeepEquals(touchpad, touchpadTesterElement.touchpad);
    // Tester element itself should not be visible in UI.
    assertFalse(isVisible(touchpadTesterElement));
    assertTrue(isChildVisible(touchpadTesterElement, titleSlotSelector));
    const titleElement =
        touchpadTesterElement.shadowRoot.querySelector('div[slot=\'title\']');
    const expectedTitle = 'Test your touchpad';
    assertElementContainsText(titleElement, expectedTitle);
    const bodyElement =
        touchpadTesterElement.shadowRoot.querySelector('div[slot=\'body\']');
    assertElementContainsText(bodyElement, touchpad.name);

    // Close tester dialog.
    touchpadTesterElement.close();
    await flushTasks();

    assertEquals(null, touchpadTesterElement.touchpad);
    assertFalse(isChildVisible(touchpadTesterElement, titleSlotSelector));
  });

  test('VerifyCanvasUpdatedWhenOnTouchEventTriggered', async () => {
    await initializeElement();
    const touchpad = fakeTouchDevices[0];
    touchpadTesterElement.show(touchpad);
    const canvas = (/** @type {HTMLCanvasElement} */ (
        touchpadTesterElement.$.testerCanvas));
    assertTrue(!!canvas);

    // Setup fake touch event data.
    const fakeTouchPoint = {
      positionX: 7,
      positionY: 102,
    };
    const fakeTouchEvent = {
      touchData: [fakeTouchPoint],
    };
    const mockController = new MockController();
    const mockDrawTrailMark = mockController.createFunctionMock(
        touchpadTesterElement.drawingProvider, 'drawTrailMark');
    mockDrawTrailMark.addExpectation(
        fakeTouchPoint.positionX, fakeTouchPoint.positionY);

    // Simulate observer being notified.
    touchpadTesterElement.onTouchEvent(fakeTouchEvent);

    // Verify touch data drawn to canvas.
    mockController.verifyMocks();
  });
});
