// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/preview_ticket_manager.js';

import {PREVIEW_REQUEST_FINISHED_EVENT, PREVIEW_REQUEST_STARTED_EVENT, PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED, PreviewTicketManager} from 'chrome://os-print/js/data/preview_ticket_manager.js';
import type {PrintPreviewPageHandlerComposite} from 'chrome://os-print/js/data/print_preview_page_handler_composite.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL, type FakePrintPreviewPageHandler, OBSERVE_PREVIEW_READY_METHOD} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {getPrintPreviewPageHandler} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {resetDataManagersAndProviders} from './test_utils.js';

suite('PreviewTicketManager', () => {
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let mockTimer: MockTimer;
  let mockController: MockController;

  setup(() => {
    // Setup fakes for testing.
    mockController = new MockController();
    mockTimer = new MockTimer();
    mockTimer.install();
    resetDataManagersAndProviders();
    printPreviewPageHandler =
        (getPrintPreviewPageHandler() as PrintPreviewPageHandlerComposite)
            .fakePageHandler;
  });

  teardown(() => {
    mockController.reset();
    mockTimer.uninstall();
    resetDataManagersAndProviders();
  });

  test('is a singleton', () => {
    const instance1 = PreviewTicketManager.getInstance();
    const instance2 = PreviewTicketManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = PreviewTicketManager.getInstance();
    PreviewTicketManager.resetInstanceForTesting();
    const instance2 = PreviewTicketManager.getInstance();
    assertTrue(instance1 !== instance2);
  });

  // Verify PrintPreviewPageHandlerComposite called when
  // sendPreviewRequest triggered.
  test(
      'sendPreviewRequest calls PrintPreviewPageHandlerComposite ' +
          'generatePreview',
      () => {
        const instance = PreviewTicketManager.getInstance();
        assertEquals(
            0, printPreviewPageHandler.getCallCount('generatePreview'));
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        assertEquals(
            1, printPreviewPageHandler.getCallCount('generatePreview'));
      });

  // Verify PREVIEW_REQUEST_STARTED_EVENT is dispatched when sendPreviewRequest
  // is called and PREVIEW_REQUEST_FINISHED_EVENT is called when the request
  // completes.
  test(
      'PREVIEW_REQUEST_STARTED_EVENT and PREVIEW_REQUEST_STARTED_EVENT are ' +
          'invoked when sendPreviewRequest called',
      async () => {
        const delay = 1;
        printPreviewPageHandler.setTestDelay(delay);
        const instance = PreviewTicketManager.getInstance();

        let startCount = 0;
        instance.addEventListener(PREVIEW_REQUEST_STARTED_EVENT, () => {
          ++startCount;
        });
        let finishCount = 0;
        instance.addEventListener(PREVIEW_REQUEST_FINISHED_EVENT, () => {
          ++finishCount;
        });
        const startEvent =
            eventToPromise(PREVIEW_REQUEST_STARTED_EVENT, instance);
        const finishEvent =
            eventToPromise(PREVIEW_REQUEST_FINISHED_EVENT, instance);

        assertEquals(0, startCount, 'Start should have zero calls');
        assertEquals(0, finishCount, 'Finish should have zero calls');

        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        await startEvent;

        assertEquals(1, startCount, 'Start should have one call');
        assertEquals(0, finishCount, 'Finish should have zero calls');

        // Advance time by test delay to trigger method resolver.
        printPreviewPageHandler.triggerOnDocumentReadyActiveRequestId();
        mockTimer.tick(delay);
        await finishEvent;

        assertEquals(1, startCount, 'Start should have one call');
        assertEquals(1, finishCount, 'Finish should have one call');
      });

  // Verify previewLoaded is false until sendPreviewRequest resolves.
  test('previewLoaded updates based on sendPreviewRequest', async () => {
    const delay = 1;
    printPreviewPageHandler.setTestDelay(delay);
    const instance = PreviewTicketManager.getInstance();
    const startEvent = eventToPromise(PREVIEW_REQUEST_STARTED_EVENT, instance);
    const finishEvent =
        eventToPromise(PREVIEW_REQUEST_FINISHED_EVENT, instance);

    assertFalse(instance.isPreviewLoaded(), 'Preview not loaded');
    instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

    await startEvent;

    assertFalse(instance.isPreviewLoaded(), 'Preview not loaded after call');

    printPreviewPageHandler.triggerOnDocumentReadyActiveRequestId();
    mockTimer.tick(delay);
    await finishEvent;

    assertTrue(instance.isPreviewLoaded(), 'Preview is loaded');
  });

  // Verify `isSessionInitialized` returns true and triggers
  // PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED event after `initializeSession`
  // called.
  test(
      'initializeSession updates isSessionInitialized and triggers ' +
          PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED,
      async () => {
        const instance = PreviewTicketManager.getInstance();
        assertFalse(
            instance.isSessionInitialized(),
            'Before initializeSession, instance should not be initialized');

        // Set initial context.
        const sessionInit = eventToPromise(
            PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await sessionInit;

        assertTrue(
            instance.isSessionInitialized(),
            'After initializeSession, instance should be initialized');
      });

  // Verify observePreviewReady is called on construction of manager.
  test('on create observePreviewReady is called', () => {
    PreviewTicketManager.getInstance();
    const expectedCallCount = 1;
    assertEquals(
        expectedCallCount,
        printPreviewPageHandler.getCallCount(OBSERVE_PREVIEW_READY_METHOD),
        `${OBSERVE_PREVIEW_READY_METHOD} called in constructor`);
  });

  // Verify PREVIEW_REQUEST_FINISHED_EVENT isn't dispatched for "stale" preview
  // requests.
  test('preview request only completes for the active request', async () => {
    const delay = 1;
    printPreviewPageHandler.setTestDelay(delay);
    const instance = PreviewTicketManager.getInstance();

    let startCount = 0;
    instance.addEventListener(
        PREVIEW_REQUEST_STARTED_EVENT, () => ++startCount);
    let finishCount = 0;
    instance.addEventListener(
        PREVIEW_REQUEST_FINISHED_EVENT, () => ++finishCount);

    let startEvent = eventToPromise(PREVIEW_REQUEST_STARTED_EVENT, instance);
    instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

    // The preview request from initialization increments the active request id.
    await startEvent;
    assertEquals(1, startCount, 'Start should have one calls');
    assertEquals(0, finishCount, 'Finish should have zero calls');

    // Triggering another preview request should again increment the active
    // request id.
    startEvent = eventToPromise(PREVIEW_REQUEST_STARTED_EVENT, instance);
    instance.sendPreviewRequest();
    await startEvent;
    assertEquals(2, startCount, 'Start should have two calls');
    assertEquals(0, finishCount, 'Finish should have zero calls');

    // Simulate the first "stale" preview request completing but it should not
    // trigger the preview complete event.
    printPreviewPageHandler.triggerOnDocumentReady(/*previewRequestId=*/ 1);
    mockTimer.tick(delay);
    assertEquals(0, finishCount, 'Finish should have zero calls');

    // Simulate the active preview request completing.
    printPreviewPageHandler.triggerOnDocumentReadyActiveRequestId();
    mockTimer.tick(delay);
    assertEquals(1, finishCount, 'Finish should have one call');
  });
});
