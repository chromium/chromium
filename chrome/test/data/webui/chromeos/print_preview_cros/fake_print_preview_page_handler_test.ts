// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';

import {FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR, FAKE_PRINT_REQUEST_SUCCESSFUL, FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('PrintPreviewCrosApp', () => {
  let printPreviewPageHandler: FakePrintPreviewPageHandler;

  setup(() => {
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    assert(printPreviewPageHandler);
  });

  // Verify initial call count for tracked methods is zero.
  test('call count zero', () => {
    assertEquals(0, printPreviewPageHandler.getCallCount('print'));
  });

  // Verify the fake PrintPreviewPageHandler returns a successful response by
  // default.
  test('default fake print request result return successful', async () => {
    const result = await printPreviewPageHandler.print();
    assertEquals(
        FAKE_PRINT_REQUEST_SUCCESSFUL, result,
        `Print request should be successful`);
  });

  // Verify the fake PrintPreviewPageHandler can set expected print request
  // result to false and resolve.
  test('can set print request result', async () => {
    printPreviewPageHandler.setPrintResult(
        FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR);
    const result = await printPreviewPageHandler.print();
    assertEquals(FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR, result);
  });
});
