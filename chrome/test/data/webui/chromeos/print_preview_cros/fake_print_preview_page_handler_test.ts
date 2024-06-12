// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';

import {DEFAULT_PARTIAL_PRINT_TICKET} from 'chrome://os-print/js/data/ticket_constants.js';
import {getFakePreviewTicket} from 'chrome://os-print/js/fakes/fake_data.js';
import {FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR, FAKE_PRINT_REQUEST_SUCCESSFUL, FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL, FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import type {PrintTicket} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';

suite('FakePrintPreviewPageHandler', () => {
  let printPreviewPageHandler: FakePrintPreviewPageHandler;

  const ticket = {...DEFAULT_PARTIAL_PRINT_TICKET} as PrintTicket;

  setup(() => {
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    assert(printPreviewPageHandler);
  });

  // Verify initial call count for tracked methods is zero.
  test('call count zero', () => {
    assertEquals(0, printPreviewPageHandler.getCallCount('print'));
    assertEquals(0, printPreviewPageHandler.getCallCount('cancel'));
    assertEquals(0, printPreviewPageHandler.getCallCount('startSession'));
  });

  // Verify the fake PrintPreviewPageHandler returns a successful response by
  // default.
  test('default fake print request result return successful', async () => {
    const result = await printPreviewPageHandler.print(ticket);
    assertEquals(
        FAKE_PRINT_REQUEST_SUCCESSFUL, result.printRequestOutcome,
        `Print request should be successful`);
  });

  // Verify the fake PrintPreviewPageHandler can set expected print request
  // result to false and resolve.
  test('can set print request result', async () => {
    printPreviewPageHandler.setPrintResult(
        FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR);
    const result = await printPreviewPageHandler.print(ticket);
    assertEquals(
        FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR,
        result.printRequestOutcome);
  });

  // Verify the fake PrintPreviewPageHandler cancel increases counter.
  test('cancel print preview can be sent', async () => {
    assertEquals(0, printPreviewPageHandler.getCallCount('cancel'));
    printPreviewPageHandler.cancel();
    assertEquals(1, printPreviewPageHandler.getCallCount('cancel'));
  });

  // Verify the fake PrintPreviewPageHandler method uses 0ms delay resolve
  // method by default.
  test('default delay is 0ms', async () => {
    const mockController = new MockController();
    const methods = printPreviewPageHandler.getMethodsForTesting();
    const resolveNoDelay =
        mockController.createFunctionMock(methods, 'resolveMethodWithDelay');
    const delay = 0;
    resolveNoDelay.addExpectation('print', delay);
    await printPreviewPageHandler.print(ticket);

    mockController.verifyMocks();
    mockController.reset();
  });

  // Verify the fake PrintPreviewPageHandler use resolve method with delay when
  // a delay is configured.
  test(
      'uses delayed resolver when testDelayMs is greater than zero',
      async () => {
        const mockController = new MockController();
        const methods = printPreviewPageHandler.getMethodsForTesting();
        const resolveWithDelay = mockController.createFunctionMock(
            methods, 'resolveMethodWithDelay');
        const delay = 1;
        resolveWithDelay.addExpectation('print', delay);
        printPreviewPageHandler.setTestDelay(delay);
        await printPreviewPageHandler.print(ticket);

        mockController.verifyMocks();
        mockController.reset();
      });

  // Verify the fake PrintPreviewHandler returns a successful SessionContext by
  // default.
  test('start session returns SessionContext', async () => {
    const result = await printPreviewPageHandler.startSession('fake-token');
    assertEquals(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL, result.sessionContext);
  });

  // Verify the fake PrintPreviewPageHandler correctly sets the preview ticket
  // when generatePreview() is called.
  test('call generate preview', async () => {
    const expectedPreviewTicket = getFakePreviewTicket();
    await printPreviewPageHandler.generatePreview(expectedPreviewTicket);
    assertEquals(
        expectedPreviewTicket, printPreviewPageHandler.getPreviewTicket());
  });
});
