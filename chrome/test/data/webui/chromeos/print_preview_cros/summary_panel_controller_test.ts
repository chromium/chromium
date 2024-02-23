// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {SummaryPanelController} from 'chrome://os-print/js/summary_panel_controller.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('SummaryPanelController', () => {
  let controller: SummaryPanelController|null = null;

  setup(() => {
    controller = new SummaryPanelController();
    assertTrue(!!controller);
  });

  teardown(() => {
    controller = null;
  });

  // Verify getSheetsUsedText returns empty string when sheet count equal to
  // zero.
  test('when zero sheets used then return an empty string', () => {
    assert(controller);
    controller.setSheetsUsedForTesting(0);
    assertEquals('', controller.getSheetsUsedText());
  });

  // Verify getSheetsUsedText returns expected string when sheet count is
  // greater than zero.
  test('when zero sheets used then return an empty string', () => {
    assert(controller);
    controller.setSheetsUsedForTesting(1);
    assertEquals(`1 used`, controller.getSheetsUsedText());
    controller.setSheetsUsedForTesting(2);
    assertEquals(`2 used`, controller.getSheetsUsedText());
  });
});
