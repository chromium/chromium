// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/print_preview_cros_app_controller.js';

import {FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {PrintPreviewCrosAppController} from 'chrome://os-print/js/print_preview_cros_app_controller.js';
import {setPrintPreviewPageHandlerForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('PrintPreviewCrosAppController', () => {
  let controller: PrintPreviewCrosAppController;
  let printPreviewPageHandler: FakePrintPreviewPageHandler;

  setup(() => {
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    setPrintPreviewPageHandlerForTesting(printPreviewPageHandler);

    controller = new PrintPreviewCrosAppController();
  });

  // Verify controller is an event target.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });

  // Verify `PrintPreviewPageHandler.startSession` is called when controller is
  // created.
  test('triggers PrintPreviewPageHandler startSession', async () => {
    // Reset call counts before creating controller.
    printPreviewPageHandler.reset();

    const method = 'startSession';
    let expectedCalls = 0;
    assertEquals(
        expectedCalls, printPreviewPageHandler.getCallCount(method),
        `No calls to start session`);

    // Controller will connect to PrintPreviewPageHandler and trigger
    // `startSession`.
    controller = new PrintPreviewCrosAppController();
    assertTrue(!!controller);
    ++expectedCalls;

    assertEquals(
        expectedCalls, printPreviewPageHandler.getCallCount(method),
        `Start session should be called once`);
  });
});
