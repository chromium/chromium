// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {SummaryPanelController} from 'chrome://os-print/js/summary_panel_controller.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';

suite('SummaryPanelController', () => {
  let controller: SummaryPanelController|null = null;
  let mockController: MockController;

  setup(() => {
    mockController = new MockController();

    controller = new SummaryPanelController();
    assertTrue(!!controller);
  });

  teardown(() => {
    mockController.reset();
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

  // Verify startPrintRequest calls PrintTicketManager.
  test(
      'calls PrintTicketManager.sendPrintRequest from handlePrintClicked',
      () => {
        const manager = PrintTicketManager.getInstance();
        const sendPrintRequestFn =
            mockController.createFunctionMock(manager, 'sendPrintRequest');
        sendPrintRequestFn.addExpectation();
        controller!.handlePrintClicked();
        mockController.verifyMocks();
      });
});
