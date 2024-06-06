// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CapabilitiesManager} from 'chrome://os-print/js/data/capabilities_manager.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {PreviewTicketManager} from 'chrome://os-print/js/data/preview_ticket_manager.js';
import {PRINT_REQUEST_FINISHED_EVENT, PRINT_TICKET_MANAGER_SESSION_INITIALIZED, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {resetProvidersForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {type Destination, PrinterStatusReason, PrinterType} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// Counter for unique destination IDs.
let destinationIdCounter = 0;

// Creates a Destination with the provided details or a generic one using the
// destinationIdCounter to provide unique ID and name.
export function createTestDestination(
    id: string = '', displayName: string = '',
    printerType: PrinterType = PrinterType.LOCAL_PRINTER): Destination {
  return {
    id: id === '' ? `destination_${++destinationIdCounter}` : id,
    displayName: displayName === '' ? `destination_${++destinationIdCounter}` :
                                      displayName,
    printerType,
    printerStatusReason: PrinterStatusReason.UNKNOWN_REASON,
  };
}

/**
 * Resets instance on all data managers and providers. Getter for providers will
 * return fakes following the reset.
 */
export function resetDataManagersAndProviders(): void {
  DestinationManager.resetInstanceForTesting();
  PrintTicketManager.resetInstanceForTesting();
  CapabilitiesManager.resetInstanceForTesting();
  PreviewTicketManager.resetInstanceForTesting();
  resetProvidersForTesting();
}

/**
 * Initializes destination manager and returns a promise for the
 * DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED event.
 * If using mockTimer, the timer and appropriate delay need to be provided to
 * resolve fetch.
 */
export function waitForInitialDestinationSet(
    mockTimer: MockTimer|null = null, delay = 0): Promise<void> {
  const destinationManager = DestinationManager.getInstance();
  const activeDestEvent = eventToPromise(
      DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
  destinationManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

  // Resolve fetch in destination manager.
  if (mockTimer) {
    mockTimer.tick(delay);
  }

  return activeDestEvent;
}

/**
 * Launches a new sendPrintRequest and returns promise for
 * PRINT_REQUEST_FINISHED_EVENT event.
 * If using mockTimer, the timer and appropriate delay need to be provided to
 * resolve fetch.
 */
export function waitForSendPrintRequestFinished(
    mockTimer: MockTimer|null = null, delay = 0): Promise<void> {
  const printTicketManager = PrintTicketManager.getInstance();
  assert(printTicketManager.isSessionInitialized());
  assert(!printTicketManager.isPrintRequestInProgress());
  const finishReqEvent =
      eventToPromise(PRINT_REQUEST_FINISHED_EVENT, printTicketManager);
  printTicketManager.sendPrintRequest();

  // Resolve print in page handler.
  if (mockTimer) {
    mockTimer.tick(delay);
  }

  return finishReqEvent;
}

/**
 * Initializes print ticket manager and returns a promise for the
 * PRINT_TICKET_MANAGER_SESSION_INITIALIZED event.
 */
export function waitForPrintTicketManagerInitialized(): Promise<void> {
  const printTicketManager = PrintTicketManager.getInstance();
  const initEvent = eventToPromise(
      PRINT_TICKET_MANAGER_SESSION_INITIALIZED, printTicketManager);
  printTicketManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
  return initEvent;
}
