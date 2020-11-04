// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/data_point.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function dataPointTestSuite() {
  /** @type {?DataPointElement} */
  let dataPointElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (dataPointElement) {
      dataPointElement.remove();
    }
    dataPointElement = null;
  });

  /**
   * @param {string} header
   * @param {string} value
   */
  function initializeDataPoint(header, value) {
    assertFalse(!!dataPointElement);

    // Add the data point to the DOM.
    dataPointElement =
        /** @type {!DataPointElement} */ (document.createElement('data-point'));
    assertTrue(!!dataPointElement);
    dataPointElement.header = header;
    dataPointElement.value = value;
    document.body.appendChild(dataPointElement);

    return flushTasks();
  }

  test('InitializeDataPoint', () => {
    const header = 'Test header';
    const value = 'Test value';
    return initializeDataPoint(header, value).then(() => {
      assertEquals(header, dataPointElement.$$('.header').textContent.trim());
      assertEquals(value, dataPointElement.$$('.value').textContent.trim());
    });
  });
}
