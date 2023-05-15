// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getStatusReasonFromPrinterStatus, PrinterStatusReason, PrinterStatusSeverity} from 'chrome://os-settings/chromeos/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('PrinterStatus', function() {
  // Verify that a printer status missing a printer id returns UNKNOWN_REASON.
  test('getStatusReasonMissingPrinterId', () => {
    const printerStatus = {
      printerId: '',
      statusReasons: [{
        reason: PrinterStatusReason.NO_ERROR,
        severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
      }],
      timestamp: 0,
    };

    assertEquals(
        PrinterStatusReason.UNKNOWN_REASON,
        getStatusReasonFromPrinterStatus(printerStatus));
  });

  // Verify that a printer status with only low severity status reasons returns
  // NO_ERROR.
  test('getStatusReasonLowSeverity', () => {
    const printerStatus = {
      printerId: 'printerId',
      statusReasons: [
        {
          reason: PrinterStatusReason.LOW_ON_INK,
          severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
        },
        {
          reason: PrinterStatusReason.PAPER_JAM,
          severity: PrinterStatusSeverity.REPORT,
        },
      ],
      timestamp: 0,
    };

    assertEquals(
        PrinterStatusReason.NO_ERROR,
        getStatusReasonFromPrinterStatus(printerStatus));
  });

  // Verify that a status reason with ERROR gets priority over WARNING severity.
  test('getStatusReasonErrorOverWarning', () => {
    const printerStatus = {
      printerId: 'printerId',
      statusReasons: [
        {
          reason: PrinterStatusReason.LOW_ON_INK,
          severity: PrinterStatusSeverity.ERROR,
        },
        {
          reason: PrinterStatusReason.PAPER_JAM,
          severity: PrinterStatusSeverity.WARNING,
        },
      ],
      timestamp: 0,
    };

    assertEquals(
        PrinterStatusReason.LOW_ON_INK,
        getStatusReasonFromPrinterStatus(printerStatus));
  });

  // Verify the last status reason with the correct severity is returned.
  test('getStatusReasonLastStatusReason', () => {
    const printerStatus = {
      printerId: 'printerId',
      statusReasons: [
        {
          reason: PrinterStatusReason.LOW_ON_INK,
          severity: PrinterStatusSeverity.WARNING,
        },
        {
          reason: PrinterStatusReason.PAPER_JAM,
          severity: PrinterStatusSeverity.WARNING,
        },
        {
          reason: PrinterStatusReason.PRINTER_UNREACHABLE,
          severity: PrinterStatusSeverity.REPORT,
        },
      ],
      timestamp: 0,
    };

    assertEquals(
        PrinterStatusReason.PAPER_JAM,
        getStatusReasonFromPrinterStatus(printerStatus));
  });
});
