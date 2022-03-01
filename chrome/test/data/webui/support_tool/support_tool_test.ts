// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the SupportToolElement. It will be executed
 * by support_tool_browsertest.js.
 */

import 'chrome://support-tool/support_tool.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {SupportToolElement} from 'chrome://support-tool/support_tool.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SupportToolTest', function() {
  let supportTool: SupportToolElement;

  const strings = {
    caseId: 'testcaseid',
  };

  setup(() => {
    loadTimeData.overrideValues(strings);
    document.body.innerHTML = '';
    supportTool = document.createElement('support-tool');
    document.body.appendChild(supportTool);
  });

  test('initialize fields', () => {
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-input')!.value, 'testcaseid');
  });
});
