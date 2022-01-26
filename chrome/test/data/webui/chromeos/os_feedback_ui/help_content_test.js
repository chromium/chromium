// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HelpContentElement} from 'chrome://os-feedback/help_content.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function helpContentTestSuite() {
  /** @type {?HelpContentElement} */
  let helpContentElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    helpContentElement.remove();
    helpContentElement = null;
  });

  function initializeHelpContentElement() {
    assertFalse(!!helpContentElement);
    helpContentElement =
        /** @type {!HelpContentElement} */ (
            document.createElement('help-content'));
    assertTrue(!!helpContentElement);
    document.body.appendChild(helpContentElement);

    return flushTasks();
  }

  /**
   * Helper function to find child element by id.
   * @param {string} id
   * */
  function getElement(id) {
    return helpContentElement.shadowRoot.querySelector(id);
  }

  /** Test that expected html elements are in the element. */
  test('HelpContentLoaded', () => {
    return initializeHelpContentElement().then(() => {
      // Verify the title is in the helpContentElement
      const title = getElement('#helpContentLabel');
      assertTrue(!!title);
      assertEquals('Suggested help content:', title.textContent);
    });
  });
}