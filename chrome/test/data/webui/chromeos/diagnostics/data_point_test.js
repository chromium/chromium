// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/data_point.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.m.js';


suite('DataPointTest', () => {
  /** @type {?HTMLElement} */
  let dataPointElement = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (dataPointElement) {
      dataPointElement.remove();
    }
    dataPointElement = null;
  });

  /**
   * @param {string} title
   * @param {string} value
   */
  function initializeDataPoint(title, value) {
    assertFalse(!!dataPointElement);

    // Add the data point to the DOM.
    dataPointElement = document.createElement('data-point');
    assertTrue(!!dataPointElement);
    document.body.appendChild(dataPointElement);
    dataPointElement.title = title;
    dataPointElement.value = value;
    return flushTasks();
  }

  test('InitializeDataPoint', () => {
    const title = 'Test title';
    const value = 'Test value';
    return initializeDataPoint(title, value).then(() => {
      assertEquals(title, dataPointElement.$$('.title').textContent.trim());
      assertEquals(value, dataPointElement.$$('.value').textContent.trim());
    });
  });
});
