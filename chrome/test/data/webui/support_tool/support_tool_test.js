// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the SupportToolElement. It will be executed
 * by support_tool_browsertest.js.
 */

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {SupportToolElement} from 'chrome://support-tool/support_tool.js';
import {assertEquals, assertTrue} from '../chai_assert.js';

suite('SupportToolTest', function() {
  var supportTool;

  setup(function() {
    document.body.innerHTML = '';
    supportTool = /** @type {!SupportToolElement} */ (
        document.createElement('support-tool'));
    document.body.appendChild(supportTool);
  });

  test('initialize checkboxes', () => {
    assertTrue(supportTool.collectBrowserLogs_);
    assertEquals(supportTool.collectChromeOSLogs_, isChromeOS);
    assertEquals(supportTool.hideChromeOS_, !isChromeOS);
  });
});
