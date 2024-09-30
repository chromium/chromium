// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationDropdownCrosElement, PrintPreviewDestinationSelectCrosElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, Error, NativeLayerCrosImpl, NativeLayerImpl, PrinterStatusReason, PrinterStatusSeverity, State} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {FakeMediaQueryList, getGoogleDriveDestination, getSaveAsPdfDestination} from './print_preview_test_utils.js';

suite('PrinterStatusTest', function() {
  const PRINTER_ICON_GREEN = 'print-preview:printer-status-green';

  const PRINTER_ICON_GREY = 'print-preview:printer-status-grey';

  const PRINTER_ICON_ORANGE = 'print-preview:printer-status-orange';

  const PRINTER_ICON_RED = 'print-preview:printer-status-red';

  let destinationSelect: PrintPreviewDestinationSelectCrosElement;

  let nativeLayerCros: NativeLayerCrosStub;

  let mockController: MockController;

  let fakePrefersColorSchemeMediaQueryList: FakeMediaQueryList;

  function setNativeLayerPrinterStatusMap() {
    [{
      printerId: 'ID1',
      statusReasons: [],
      timestamp: 0,
    },
     {
       printerId: 'ID2',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
       }],
       timestamp: 0,
     },
     {
       printerId: 'ID3',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.REPORT,
       }],
       timestamp: 0,
     },
     {
       printerId: 'ID4',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.WARNING,
       }],
       timestamp: 0,
     },
     {
       printerId: 'ID5',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.ERROR,
       }],
       timestamp: 0,
     },
     {
       printerId: 'ID6',
       statusReasons: [
         {
           reason: PrinterStatusReason.DEVICE_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
         },
         {
           reason: PrinterStatusReason.PRINTER_QUEUE_FULL,
           severity: PrinterStatusSeverity.ERROR,
         },
       ],
       timestamp: 0,
     },
     {
       printerId: 'ID7',
       statusReasons: [
         {
           reason: PrinterStatusReason.DEVICE_ERROR,
           severity: PrinterStatusSeverity.REPORT,
         },
         {
           reason: PrinterStatusReason.PRINTER_QUEUE_FULL,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
         },
       ],
       timestamp: 0,
     },
     {
       printerId: 'ID8',
       statusReasons: [{
         reason: PrinterStatusReason.UNKNOWN_REASON,
         severity: PrinterStatusSeverity.ERROR,
       }],
       timestamp: 0,
     },
     {
       printerId: 'ID9',
       statusReasons: [{
         reason: PrinterStatusReason.UNKNOWN_REASON,
         severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
       }],
       timestamp: 0,
     },
     {
       printerId: 'ID10',
       statusReasons: [{
         reason: PrinterStatusReason.PRINTER_UNREACHABLE,
         severity: PrinterStatusSeverity.ERROR,
       }],
       timestamp: 0,
     }]
        .forEach(
            status => nativeLayerCros.addPrinterStatusToMap(
                status.printerId, status));
  }

  function createDestination(
      id: string, displayName: string,
      destinationOrigin: DestinationOrigin): Destination {
    return new Destination(id, destinationOrigin, displayName);
  }

  function escapeForwardSlahes(value: string): string {
    return value.replace(/\//g, '\\/');
  }

  function getIconString(
      dropdown: PrintPreviewDestinationDropdownCrosElement,
      key: string): string {
    return dropdown.shadowRoot!.querySelector(`#${
        escapeForwardSlahes(key)}`)!.querySelector('cr-icon')!.icon!;
  }

  // Mocks calls to window.matchMedia, returning false by default.
  function configureMatchMediaMock() {
    mockController = new MockController();
    const matchMediaMock =
        mockController.createFunctionMock(window, 'matchMedia');
    fakePrefersColorSchemeMediaQueryList =
        new FakeMediaQueryList('(prefers-color-scheme: dark)');
    matchMediaMock.returnValue = fakePrefersColorSchemeMediaQueryList;
    assertFalse(window.matchMedia('(prefers-color-scheme: dark)').matches);
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    configureMatchMediaMock();

    // Stub out native layer.
    NativeLayerImpl.setInstance(new NativeLayerStub());
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.setInstance(nativeLayerCros);
    setNativeLayerPrinterStatusMap();

    destinationSelect =
        document.createElement('print-preview-destination-select-cros');
    document.body.appendChild(destinationSelect);
  });

  teardown(function() {
    mockController.reset();
  });

  /**
   * Verifies icon contains expected color depending on destination's
   * computed printer status reason from `getStatusReasonFromPrinterStatus`.
   */
  test('PrinterStatusUpdatesColor', function() {
    const destination1 =
        createDestination('ID1', 'One', DestinationOrigin.CROS);
    const destination2 =
        createDestination('ID2', 'Two', DestinationOrigin.CROS);
    const destination3 =
        createDestination('ID3', 'Three', DestinationOrigin.CROS);
    const destination4 =
        createDestination('ID4', 'Four', DestinationOrigin.CROS);
    const destination5 =
        createDestination('ID5', 'Five', DestinationOrigin.CROS);
    const destination6 =
        createDestination('ID6', 'Six', DestinationOrigin.CROS);
    const destination7 =
        createDestination('ID7', 'Seven', DestinationOrigin.CROS);
    const destination8 =
        createDestination('ID8', 'Eight', DestinationOrigin.CROS);
    const destination9 =
        createDestination('ID9', 'Nine', DestinationOrigin.CROS);

    return waitBeforeNextRender(destinationSelect)
        .then(() => {
          const whenStatusRequestsDone =
              nativeLayerCros.waitForMultiplePrinterStatusRequests(9);

          destinationSelect.recentDestinationList = [
            destination1,
            destination2,
            destination3,
            destination4,
            destination5,
            destination6,
            destination7,
            destination8,
            destination9,
          ];

          return whenStatusRequestsDone;
        })
        .then(() => {
          return waitBeforeNextRender(destinationSelect);
        })
        .then(() => {
          const dropdown = destinationSelect.$.dropdown;

          // Empty printer status.
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination1.key));

          // Error printer status with unknown severity.
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination2.key));

          // Error printer status with report severity.
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination3.key));

          // Error printer status with warning severity.
          assertEquals(
              PRINTER_ICON_ORANGE, getIconString(dropdown, destination4.key));

          // Error printer status with error severity.
          assertEquals(
              PRINTER_ICON_ORANGE, getIconString(dropdown, destination5.key));

          // Error printer status with unknown severity + error printer
          // status with error severity.
          assertEquals(
              PRINTER_ICON_ORANGE, getIconString(dropdown, destination6.key));

          // Error printer status with unknown severity + error printer
          // status with report severity.
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination7.key));

          // Unknown reason printer status with error severity.
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination8.key));

          // Unknown reason printer status with unknown severity.
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination9.key));
        });
  });

  test('SendStatusRequestOnce', function() {
    return waitBeforeNextRender(destinationSelect).then(() => {
      const destination1 =
          createDestination('ID1', 'One', DestinationOrigin.CROS);
      const destination2 =
          createDestination('ID2', 'Two', DestinationOrigin.CROS);

      destinationSelect.recentDestinationList = [
        destination1,
        destination2,
        createDestination('ID3', 'Three', DestinationOrigin.EXTENSION),
        createDestination('ID4', 'Four', DestinationOrigin.EXTENSION),
      ];
      assertEquals(
          2, nativeLayerCros.getCallCount('requestPrinterStatusUpdate'));

      // Update list with 2 existing destinations and one new destination.
      // Make sure the requestPrinterStatusUpdate only gets called for the
      // new destination.
      destinationSelect.recentDestinationList = [
        destination1,
        destination2,
        createDestination('ID5', 'Five', DestinationOrigin.CROS),
      ];
      assertEquals(
          3, nativeLayerCros.getCallCount('requestPrinterStatusUpdate'));
    });
  });

  test('HiddenStatusText', function() {
    const destinationStatus =
        destinationSelect.shadowRoot!.querySelector<HTMLElement>(
            '.destination-additional-info')!;
    return waitBeforeNextRender(destinationSelect)
        .then(() => {
          const destinationWithoutErrorStatus =
              createDestination('ID1', 'One', DestinationOrigin.CROS);
          // Destination with ID4 will return an error printer status that will
          // trigger the error text being populated.
          const destinationWithErrorStatus =
              createDestination('ID4', 'Four', DestinationOrigin.CROS);
          destinationSelect.recentDestinationList = [
            destinationWithoutErrorStatus,
            destinationWithErrorStatus,
          ];

          const destinationEulaWrapper =
              destinationSelect.$.destinationEulaWrapper;

          destinationSelect.destination = destinationWithoutErrorStatus;
          assertTrue(destinationStatus.hidden);
          assertTrue(destinationEulaWrapper.hidden);

          destinationSelect.set(
              'destination.eulaUrl', 'chrome://os-credits/eula');
          assertFalse(destinationEulaWrapper.hidden);

          destinationSelect.destination = destinationWithErrorStatus;
          return nativeLayerCros.whenCalled('requestPrinterStatusUpdate');
        })
        .then(() => {
          return waitBeforeNextRender(destinationSelect);
        })
        .then(() => {
          assertFalse(destinationStatus.hidden);
        });
  });

  test('ChangeIcon', function() {
    return waitBeforeNextRender(destinationSelect).then(() => {
      const localCrosPrinter =
          createDestination('ID1', 'One', DestinationOrigin.CROS);
      const localNonCrosPrinter =
          createDestination('ID2', 'Two', DestinationOrigin.LOCAL);
      const crosEnterprisePrinter = new Destination(
          'ID5', DestinationOrigin.CROS, 'Five', {isEnterprisePrinter: true});
      const saveToDrive = getGoogleDriveDestination();
      const saveAsPdf = getSaveAsPdfDestination();

      destinationSelect.recentDestinationList = [
        localCrosPrinter,
        saveToDrive,
        saveAsPdf,
      ];
      const dropdown = destinationSelect.$.dropdown;

      destinationSelect.destination = localCrosPrinter;
      destinationSelect.updateDestination();
      assertEquals(PRINTER_ICON_GREY, dropdown.destinationIcon);

      destinationSelect.destination = localNonCrosPrinter;
      destinationSelect.updateDestination();
      assertEquals('print-preview:print', dropdown.destinationIcon);

      destinationSelect.destination = crosEnterprisePrinter;
      destinationSelect.updateDestination();
      assertEquals(
          'print-preview:business-printer-status-grey',
          dropdown.destinationIcon);

      destinationSelect.destination = saveToDrive;
      destinationSelect.updateDestination();
      assertEquals('print-preview:save-to-drive', dropdown.destinationIcon);

      destinationSelect.destination = saveAsPdf;
      destinationSelect.updateDestination();
      assertEquals('cr:insert-drive-file', dropdown.destinationIcon);
    });
  });

  test('SuccessfulPrinterStatusAfterRetry', function() {
    nativeLayerCros.simulateStatusRetrySuccesful();

    const destination =
        createDestination('ID10', 'Ten', DestinationOrigin.CROS);
    destination.setPrinterStatusRetryTimeoutForTesting(100);
    const whenStatusRequestsDonePromise =
        nativeLayerCros.waitForMultiplePrinterStatusRequests(2);
    destinationSelect.recentDestinationList = [
      destination,
    ];

    const dropdown = destinationSelect.$.dropdown;
    return whenStatusRequestsDonePromise
        .then(() => {
          assertEquals(
              PRINTER_ICON_GREY, getIconString(dropdown, destination.key));
          return waitBeforeNextRender(destinationSelect);
        })
        .then(() => {
          // The printer status is requested twice because of the retry.
          assertEquals(
              2, nativeLayerCros.getCallCount('requestPrinterStatusUpdate'));
          assertEquals(
              PRINTER_ICON_GREEN, getIconString(dropdown, destination.key));
        });
  });

  test('StatusTextClass', async function() {
    const destinationStatus =
        destinationSelect.shadowRoot!.querySelector<HTMLElement>(
            '.destination-additional-info')!;
    await waitBeforeNextRender(destinationSelect);
    const destinationWithoutErrorStatus =
        createDestination('ID1', 'One', DestinationOrigin.CROS);
    const destinationWithErrorStatus1 =
        createDestination('ID4', 'Four', DestinationOrigin.CROS);
    const destinationWithErrorStatus2 =
        createDestination('ID10', 'Ten', DestinationOrigin.CROS);
    destinationSelect.recentDestinationList = [
      destinationWithoutErrorStatus,
      destinationWithErrorStatus1,
      destinationWithErrorStatus2,
    ];

    // Destination with ID1 will not have an error status shown.
    destinationSelect.destination = destinationWithoutErrorStatus;
    destinationSelect.updateDestination();
    await destinationSelect.destination.requestPrinterStatus();
    await waitBeforeNextRender(destinationSelect);

    const statusText: HTMLElement =
        destinationStatus.querySelector<HTMLElement>('#statusText')!;
    assertEquals('destination-status', statusText.className.trim());

    // Destination with ID4 will return an error printer status that will
    // trigger the error text being populated with status-orange class.
    destinationSelect.destination = destinationWithErrorStatus1;
    await destinationSelect.destination.requestPrinterStatus();
    await waitBeforeNextRender(destinationSelect);

    assertTrue(
        statusText.classList.contains('status-orange'), 'contains orange');

    // Destination with ID10 will return an error printer status that will
    // trigger the error text being populated with status-red class
    destinationSelect.destination = destinationWithErrorStatus2;
    await destinationSelect.destination.requestPrinterStatus();
    await waitBeforeNextRender(destinationSelect);

    assertTrue(
        statusText.classList.contains('status-red'),
        `contains red: ${statusText.className}`);
  });

  test('OverrideStatusForPrinterSetupInfo', async function() {
    await waitBeforeNextRender(destinationSelect);

    // Set to a destination without printer status errors.
    const destinationNoError =
        createDestination('ID1', 'One', DestinationOrigin.CROS);
    destinationSelect.recentDestinationList = [destinationNoError];
    await nativeLayerCros.whenCalled('requestPrinterStatusUpdate');
    destinationSelect.destination = destinationNoError;
    destinationSelect.updateDestination();

    // Confirm there is no error text and the printer status icon is green.
    const destinationStatus =
        destinationSelect.shadowRoot!.querySelector<HTMLElement>(
            '.destination-additional-info')!;
    const statusText: HTMLElement =
        destinationStatus.querySelector<HTMLElement>('#statusText')!;
    assertEquals('', statusText.textContent);
    const dropdown = destinationSelect.$.dropdown;
    assertEquals(PRINTER_ICON_GREEN, dropdown.destinationIcon);

    // Set the preview in an error state that shows the printer setup info UI
    // then verify the status text and icon update accordingly.
    destinationSelect.state = State.ERROR;
    destinationSelect.error = Error.INVALID_PRINTER;
    assertEquals('Not connected to printer', statusText.textContent!.trim());
    assertEquals(PRINTER_ICON_RED, dropdown.destinationIcon);
  });
});
