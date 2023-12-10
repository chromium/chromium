// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageToolbarElement} from 'chrome://resources/ash/common/page_toolbar.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('pageToolbarTestSuite', () => {
  /** @type {?PageToolbarElement} */
  let pageToolbarElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    pageToolbarElement.remove();
    pageToolbarElement = null;
  });

  /**
   * @param {string} title
   * @return {!Promise}
   */
  function initializePageToolbar(title) {
    assertFalse(!!pageToolbarElement);

    pageToolbarElement =
        /** @type {!PageToolbarElement} */ (
            document.createElement('page-toolbar'));

    assertTrue(!!pageToolbarElement);
    pageToolbarElement.title = title;
    document.body.appendChild(pageToolbarElement);

    return flushTasks();
  }

  test('TitleInitialized', () => {
    return initializePageToolbar('title').then(() => {
      assertTrue(isVisible(/** @type {!HTMLElement} */ (
          pageToolbarElement.shadowRoot.querySelector('#title'))));
      const expectedTitle = 'title';
      const actualTitle = pageToolbarElement.shadowRoot.querySelector('#title')
                              .textContent.trim();
      assertEquals(expectedTitle, actualTitle);
    });
  });
});
