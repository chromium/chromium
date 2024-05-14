// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
