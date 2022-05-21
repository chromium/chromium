// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileAttachmentElement} from 'chrome://os-feedback/file_attachment.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function fileAttachmentTestSuite() {
  /** @type {?FileAttachmentElement} */
  let page = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!FileAttachmentElement} */ (
            document.createElement('file-attachment'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /**
   * @param {string} selector
   * @returns {Element|null}
   */
  function getElement(selector) {
    const element = page.shadowRoot.querySelector(selector);
    return element;
  }

  /**
   * @param {string} selector
   * @returns {string}
   */
  function getElementContent(selector) {
    const element = getElement(selector);
    assertTrue(!!element);
    return element.textContent.trim();
  }

  // Test the page is loaded with expected HTML elements.
  test('shareDataPageLoaded', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    assertEquals('Add file', getElementContent('#AddFileLabel'));
  });
}
