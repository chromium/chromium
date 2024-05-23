// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CapabilitiesManager} from 'chrome://os-print/js/data/capabilities_manager.js';
import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {PreviewTicketManager} from 'chrome://os-print/js/data/preview_ticket_manager.js';
import {PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {resetProvidersForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {type Destination, PrinterStatusReason, PrinterType} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';

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
    printerManuallySelected: false,
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
