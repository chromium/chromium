// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PrinterListEntry, PrinterSettingsUserAction, PrinterStatusReason, PrinterType, SettingsCupsPrintersEntryElement} from 'chrome://os-settings/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

function createPrinterEntry(
    printerType: PrinterType, isManaged: boolean = true): PrinterListEntry {
  return {
    printerInfo: {
      isManaged: isManaged,
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerMakeAndModel: '',
      printerName: 'Test name',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printServerUri: '',
    },
    printerType: printerType,
  };
}

suite('<settings-cups-printers-entry>', () => {
  /** A printer list entry created before each test. */
  let printerEntryTestElement: SettingsCupsPrintersEntryElement;

  setup(() => {
    initializePrinterEntryTestElement();
  });

  teardown(() => {
    printerEntryTestElement.remove();
  });

  function initializePrinterEntryTestElement() {
    if (printerEntryTestElement) {
      printerEntryTestElement.remove();
    }
    printerEntryTestElement =
        document.createElement('settings-cups-printers-entry');
    assertTrue(!!printerEntryTestElement);
    printerEntryTestElement.printerStatusReasonCache = new Map();
    document.body.appendChild(printerEntryTestElement);
  }

  test('initializePrinterEntry', () => {
    const expectedPrinterName = 'Test name';

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);
    const entryText =
        printerEntryTestElement.shadowRoot!.querySelector('.entry-text');
    assertTrue(!!entryText);
    assertEquals(expectedPrinterName, entryText.textContent!.trim());
  });

  test('savedPrinterShowsThreeDotMenu', () => {
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);

    // Assert that three dot menu is not shown before the dom is updated.
    assertNull(
        printerEntryTestElement.shadowRoot!.querySelector('.icon-more-vert'));

    flush();

    // Three dot menu should be visible when |printerType| is set to
    // PrinterType.SAVED.
    assertTrue(
        !!printerEntryTestElement.shadowRoot!.querySelector('.icon-more-vert'));
  });

  test('disableButtonWhenSavingPrinterOrDisallowedByPolicy', () => {
    const printerTypes = [
      PrinterType.DISCOVERED,
      PrinterType.AUTOMATIC,
      PrinterType.PRINTSERVER,
    ];
    const printerIds = [
      '#setupPrinterButton',
      '#automaticPrinterButton',
      '#savePrinterButton',
    ];
    for (let i = 0; i < printerTypes.length; i++) {
      printerEntryTestElement.printerEntry =
          createPrinterEntry(printerTypes[i]!);
      flush();
      const actionButton =
          printerEntryTestElement.shadowRoot!.querySelector<HTMLButtonElement>(
              printerIds[i]!);
      assertTrue(!!actionButton);
      printerEntryTestElement.savingPrinter = true;
      printerEntryTestElement.userPrintersAllowed = true;
      assertTrue(actionButton.disabled);

      printerEntryTestElement.savingPrinter = true;
      printerEntryTestElement.userPrintersAllowed = false;
      assertTrue(actionButton.disabled);

      printerEntryTestElement.savingPrinter = false;
      printerEntryTestElement.userPrintersAllowed = true;
      assertFalse(actionButton.disabled);

      printerEntryTestElement.savingPrinter = false;
      printerEntryTestElement.userPrintersAllowed = false;
      assertTrue(actionButton.disabled);
    }
  });

  // Verify the correct printer status icon is shown based on the printer's
  // status reason.
  test('savedPrinterCorrectPrinterStatusIcon', () => {
    initializePrinterEntryTestElement();
    const printerStatusReasonCache = new Map();
    printerStatusReasonCache.set('id1', PrinterStatusReason.NO_ERROR);
    printerStatusReasonCache.set('id2', PrinterStatusReason.OUT_OF_PAPER);
    printerStatusReasonCache.set(
        'id3', PrinterStatusReason.PRINTER_UNREACHABLE);
    printerStatusReasonCache.set('id4', PrinterStatusReason.UNKNOWN_REASON);

    printerEntryTestElement.printerStatusReasonCache = printerStatusReasonCache;
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);

    const printerStatusIcon =
        printerEntryTestElement.shadowRoot!.querySelector<IronIconElement>(
            '#printerStatusIcon');
    assertTrue(!!printerStatusIcon);
    const printerSubtext =
        printerEntryTestElement.shadowRoot!.querySelector('#printerSubtext');
    assertTrue(!!printerSubtext);

    // Start at the unknown state.
    assertEquals(
        'os-settings:printer-status-illo-grey', printerStatusIcon.icon);
    assertEquals('', printerSubtext.textContent?.trim());

    // Set to an low severity error status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id2');
    assertEquals(
        'os-settings:printer-status-illo-orange', printerStatusIcon.icon);
    assertEquals(
        loadTimeData.getString('printerStatusOutOfPaper'),
        printerSubtext.textContent!.trim());

    // Set to a good status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id1');
    assertEquals(
        'os-settings:printer-status-illo-green', printerStatusIcon.icon);
    assertEquals('', printerSubtext.textContent?.trim());

    // Set to a high severity error status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id3');
    assertEquals('os-settings:printer-status-illo-red', printerStatusIcon.icon);
    assertEquals(
        loadTimeData.getString('printerStatusPrinterUnreachable'),
        printerSubtext.textContent!.trim());

    // Set to unknown status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id4');
    assertEquals(
        'os-settings:printer-status-illo-green', printerStatusIcon.icon);
    assertEquals('', printerSubtext.textContent?.trim());
  });

  // Verify the printer icon is visible based on the printer's isManaged info.
  test('visiblePrinterIconByPrinterInfo', () => {
    printerEntryTestElement.printerEntry = createPrinterEntry(
        /*printerType=*/ PrinterType.ENTERPRISE, /*isManaged=*/ true);
    assertFalse(isVisible(printerEntryTestElement.shadowRoot!.querySelector(
        '#printerStatusIcon')));

    printerEntryTestElement.printerEntry = createPrinterEntry(
        /*printerType=*/ PrinterType.DISCOVERED, /*isManaged=*/ false);
    assertTrue(isVisible(printerEntryTestElement.shadowRoot!.querySelector(
        '#printerStatusIcon')));

    printerEntryTestElement.printerEntry = createPrinterEntry(
        /*printerType=*/ PrinterType.AUTOMATIC, /*isManaged=*/ false);
    assertTrue(isVisible(printerEntryTestElement.shadowRoot!.querySelector(
        '#printerStatusIcon')));

    printerEntryTestElement.printerEntry = createPrinterEntry(
        /*printerType=*/ PrinterType.SAVED, /*isManaged=*/ false);
    assertTrue(isVisible(printerEntryTestElement.shadowRoot!.querySelector(
        '#printerStatusIcon')));
  });

  // Verify clicking the setup or save button is recorded to metrics.
  test('recordUserActionMetric', () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    // Enable the save printer buttons.
    printerEntryTestElement.savingPrinter = false;
    printerEntryTestElement.userPrintersAllowed = true;

    // Verify saving an automatic printer is recorded.
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.AUTOMATIC);
    flush();
    printerEntryTestElement.shadowRoot!
        .querySelector<HTMLElement>('#automaticPrinterButton')!.click();
    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'Printing.CUPS.SettingsUserAction',
            PrinterSettingsUserAction.SAVE_PRINTER));

    // Verify saving a discovered printer is recorded.
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.DISCOVERED);
    flush();
    printerEntryTestElement.shadowRoot!
        .querySelector<HTMLElement>('#setupPrinterButton')!.click();
    assertEquals(
        2,
        fakeMetricsPrivate.countMetricValue(
            'Printing.CUPS.SettingsUserAction',
            PrinterSettingsUserAction.SAVE_PRINTER));
  });

  // Verify the correct ARIA label is computed based on the entry attributes.
  test('entryAriaLabel', () => {
    initializePrinterEntryTestElement();

    const printerStatusReasonCache = new Map();
    printerStatusReasonCache.set('id1', PrinterStatusReason.OUT_OF_INK);
    printerEntryTestElement.printerStatusReasonCache = printerStatusReasonCache;
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);

    const printerName = 'Test name';
    const index = 0;
    const numPrinters = 3;
    printerEntryTestElement.focusRowIndex = index;
    printerEntryTestElement.numPrinters = numPrinters;
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id1');

    flush();
    assertEquals(
        loadTimeData.getStringF(
            'printerEntryAriaLabel', printerName, 'Out of ink', index + 1,
            numPrinters),
        printerEntryTestElement.shadowRoot!
            .querySelector<HTMLElement>('#entry')!.ariaLabel!.trim());
  });

  // Verify the correct button label is shown for discovered printers.
  test('discoveredPrintersButtonLabel', () => {
    initializePrinterEntryTestElement();

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.DISCOVERED);
    flush();
    assertEquals(
        loadTimeData.getString('savePrinter'),
        printerEntryTestElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#setupPrinterButton')!.textContent!.trim());
  });
});
