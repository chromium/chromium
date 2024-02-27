// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/print_ticket_manager.js';

import {PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {setPrintPreviewPageHandlerForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('PrintTicketManager', () => {
  let printPreviewPageHandler: FakePrintPreviewPageHandler;

  setup(() => {
    PrintTicketManager.resetInstanceForTesting();

    // Setup fakes for testing.
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    setPrintPreviewPageHandlerForTesting(printPreviewPageHandler);
  });

  test('is a singleton', () => {
    const instance1 = PrintTicketManager.getInstance();
    const instance2 = PrintTicketManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = PrintTicketManager.getInstance();
    PrintTicketManager.resetInstanceForTesting();
    const instance2 = PrintTicketManager.getInstance();
    assertTrue(instance1 !== instance2);
  });

  // Verify PrintPreviewPageHandler called when sentPrintRequest triggered.
  test('sendPrintRequest calls PrintPreviewPageHandler.print', () => {
    const instance = PrintTicketManager.getInstance();
    assertEquals(0, printPreviewPageHandler.getCallCount('print'));
    instance.sendPrintRequest();
    assertEquals(1, printPreviewPageHandler.getCallCount('print'));
  });
});
