// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/data_point.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

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
   * @param {string=} tooltipText
   * @param {boolean=} warningState
   */
  function initializeDataPoint(
      header, value, tooltipText = '', warningState = false) {
    assertFalse(!!dataPointElement);

    // Add the data point to the DOM.
    dataPointElement =
        /** @type {!DataPointElement} */ (document.createElement('data-point'));
    assertTrue(!!dataPointElement);
    dataPointElement.header = header;
    dataPointElement.value = value;
    dataPointElement.tooltipText = tooltipText;
    dataPointElement.warningState = warningState;
    document.body.appendChild(dataPointElement);

    return flushTasks();
  }

  test('InitializeDataPoint', () => {
    const header = 'Test header';
    const value = 'Test value';
    const tooltipText = 'Test tooltip';
    return initializeDataPoint(header, value, tooltipText).then(() => {
      dx_utils.assertElementContainsText(
          dataPointElement.$$('.header > span'), header);
      dx_utils.assertElementContainsText(dataPointElement.$$('.value'), value);
      assertTrue(isVisible(
          /**@type {!HTMLElement} */ (dataPointElement.$$('#infoIcon'))));
      dx_utils.assertElementContainsText(
          dataPointElement.$$('paper-tooltip'), tooltipText);
    });
  });

  test('InitializeDataPointWithoutTooltip', () => {
    const header = 'Test header';
    const value = 'Test value';
    return initializeDataPoint(header, value).then(() => {
      // Icon should be hidden when tooltip text is not provided.
      assertFalse(isVisible(
          /**@type {!HTMLElement} */ (dataPointElement.$$('#infoIcon'))));
    });
  });

  test('InitializeDataPointWithWarningState', () => {
    const header = 'Test header';
    const value = 'Test value';
    return initializeDataPoint(header, value, '', true).then(() => {
      dx_utils.assertElementContainsText(
          dataPointElement.$$('.text-red'), value);
    });
  });
}
