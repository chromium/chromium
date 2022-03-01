// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the SupportToolElement. It will be executed
 * by support_tool_browsertest.js.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {SupportToolElement} from 'chrome://support-tool/support_tool.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SupportToolTest', function() {
  var supportTool;

  const strings = {
    caseId: 'testcaseid',
  };

  setup(function() {
    loadTimeData.overrideValues(strings);
    document.body.innerHTML = '';
    supportTool = /** @type {!SupportToolElement} */ (
        document.createElement('support-tool'));
    document.body.appendChild(supportTool);
  });

  test('initialize fields', () => {
    assertEquals(
        supportTool.shadowRoot.getElementById('support-case-id').value,
        'testcaseid');
  });
});
