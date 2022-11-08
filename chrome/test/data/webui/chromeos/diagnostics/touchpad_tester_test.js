// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {TouchpadTesterElement} from 'chrome://diagnostics/touchpad_tester.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {isChildVisible} from '../../test_util.js';

suite('touchpadTesterTestSuite', function() {
  /** @type {?TouchpadTesterElement} */
  let touchpadTesterElement = null;

  setup(() => {
    document.body.innerHTML = '';
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
    const titleSlotSelector = '#touchpadTesterDialog div[slot=title]';
    assertFalse(isChildVisible(touchpadTesterElement, titleSlotSelector));
  });
});
