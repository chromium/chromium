// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/data_point.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {DataPointElement} from 'chrome://diagnostics/data_point.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('dataPointTestSuite', function() {
  let dataPointElement: DataPointElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    dataPointElement?.remove();
    dataPointElement = null;
  });

  function initializeDataPoint(
      header: string, value: string, tooltipText: string = '',
      warningState: boolean = false): Promise<void> {
    assertFalse(!!dataPointElement);

    // Add the data point to the DOM.
    dataPointElement = document.createElement('data-point');
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
          dataPointElement!.shadowRoot!.querySelector('.header > span'),
          header);
      dx_utils.assertElementContainsText(
          dataPointElement!.shadowRoot!.querySelector('.value'), value);
      assertTrue(
          isVisible(dataPointElement!.shadowRoot!.querySelector('#infoIcon')));
      dx_utils.assertElementContainsText(
          dataPointElement!.shadowRoot!.querySelector('paper-tooltip'),
          tooltipText);
    });
  });

  test('InitializeDataPointWithoutTooltip', () => {
    const header = 'Test header';
    const value = 'Test value';
    return initializeDataPoint(header, value).then(() => {
      // Icon should be hidden when tooltip text is not provided.
      assertFalse(
          isVisible(dataPointElement!.shadowRoot!.querySelector('#infoIcon')));
    });
  });

  test('InitializeDataPointWithWarningState', () => {
    const header = 'Test header';
    const value = 'Test value';
    return initializeDataPoint(header, value, '', true).then(() => {
      dx_utils.assertElementContainsText(
          dataPointElement!.shadowRoot!.querySelector('.text-red'), value);
    });
  });
});
