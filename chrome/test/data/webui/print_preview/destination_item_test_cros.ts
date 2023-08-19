// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationOrigin, NativeLayerCrosImpl, PrinterStatusReason, PrinterStatusSeverity, PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {FakeMediaQueryList} from './print_preview_test_utils.js';

const destination_item_test_cros = {
  suiteName: 'DestinationItemTestCros',
  TestNames: {
    NewStatusUpdatesIcon: 'new status updates icon',
    ChangingDestinationUpdatesIcon: 'changing destination updates icon',
    OnlyUpdateMatchingDestination: 'only update matching destination',
    // TODO(b/289091283): Remove test for flag off and update test name for flag
    //                    on when `isPrintPreviewSetupAssistanceEnabled` flag is
    //                    removed.
    PrinterIconMapsToPrinterStatus_FlagOff: 'printer icon maps to printer ' +
        'status with isPrintPreviewSetupAssistanceEnabled flag off',
    PrinterIconMapsToPrinterStatus_FlagOn: 'printer icon maps to printer ' +
        'status with isPrintPreviewSetupAssistanceEnabled flag on',
    // TODO(b/289091283): Remove test for flag off and update test name for flag
    //                    on when `isPrintPreviewSetupAssistanceEnabled` flag is
    //                    removed.
    PrinterConnectionStatusClass_FlagOff: 'printer connection status class ' +
        'with isPrintPreviewSetupAssistanceEnabled flag off',
    PrinterConnectionStatusClass_FlagOn: 'printer connection status class ' +
        'with isPrintPreviewSetupAssistanceEnabled flag on',
  },
};

Object.assign(window, {destination_item_test_cros: destination_item_test_cros});

suite(destination_item_test_cros.suiteName, function() {
  let listItem: PrintPreviewDestinationListItemElement;

  let nativeLayerCros: NativeLayerCrosStub;

  let mockController: MockController;

  let fakePrefersColorSchemeMediaQueryList: FakeMediaQueryList;

  function setNativeLayerPrinterStatusMap() {
    [{
      printerId: 'One',
      statusReasons: [{
        reason: PrinterStatusReason.NO_ERROR,
        severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
      }],
      timestamp: 0,
    },
     {
       printerId: 'Two',
       statusReasons: [{
         reason: PrinterStatusReason.OUT_OF_INK,
         severity: PrinterStatusSeverity.ERROR,
       }],
       timestamp: 0,
     },
     {
       printerId: 'Three',
       statusReasons: [
         {
           reason: PrinterStatusReason.PRINTER_UNREACHABLE,
           severity: PrinterStatusSeverity.ERROR,
         },
       ],
       timestamp: 0,
     },
     {
       printerId: 'Four',
       statusReasons: [{
         reason: PrinterStatusReason.UNKNOWN_REASON,
         severity: PrinterStatusSeverity.ERROR,
       }],
       timestamp: 0,
     }]
        .forEach(
            status => nativeLayerCros.addPrinterStatusToMap(
                status.printerId, status));
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

  function createTestDestination(printerId: string): Destination {
    return new Destination(
        printerId, DestinationOrigin.CROS, `Destination ${printerId}`,
        {description: 'ABC'});
  }

  setup(function() {
    // Mock configuration needs to happen before element added to UI to
    // ensure iron-media-query uses mock.
    configureMatchMediaMock();

    // Default setup assistance flag to off.
    loadTimeData.overrideValues({
      isPrintPreviewSetupAssistanceEnabled: false,
    });

    document.body.innerHTML = getTrustedHTML`
          <print-preview-destination-list-item id="listItem">
          </print-preview-destination-list-item>`;

    // Stub out native layer.
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.setInstance(nativeLayerCros);
    setNativeLayerPrinterStatusMap();

    listItem =
        document.body.querySelector<PrintPreviewDestinationListItemElement>(
            '#listItem')!;
    listItem.destination = new Destination(
        'One', DestinationOrigin.CROS, 'Destination One', {description: 'ABC'});
    flush();
  });

  teardown(function() {
    mockController.reset();
  });

  test(
      destination_item_test_cros.TestNames.NewStatusUpdatesIcon, function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);

        return listItem.destination.requestPrinterStatus().then(() => {
          assertEquals('print-preview:printer-status-green', icon.icon);
        });
      });

  test(
      destination_item_test_cros.TestNames.ChangingDestinationUpdatesIcon,
      function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);

        listItem.destination = new Destination(
            'Two', DestinationOrigin.CROS, 'Destination Two',
            {description: 'ABC'});

        // TODO(b/289091283): Update expected icon to be
        // `print-preview:printer-status-orange` when flag is removed.
        return waitBeforeNextRender(listItem).then(() => {
          assertEquals('print-preview:printer-status-red', icon.icon);
        });
      });

  // Tests that the printer stauts icon is only notified to update if the
  // destination key in the printer status response matches the current
  // destination.
  test(
      destination_item_test_cros.TestNames.OnlyUpdateMatchingDestination,
      function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);
        const firstDestinationStatusRequestPromise =
            listItem.destination.requestPrinterStatus();

        // Simulate destination_list updating and switching the destination
        // after the request for the original destination was already sent out.
        listItem.destination = new Destination(
            'Two', DestinationOrigin.CROS, 'Destination Two',
            {description: 'ABC'});

        return firstDestinationStatusRequestPromise.then(() => {
          // PrinterState should stay the same because the destination in the
          // status request response doesn't match.
          assertEquals('print-preview:printer-status-grey', icon.icon);
        });
      });

  // Verifies expected icon displays for given status when
  // `isPrintPreviewSetupAssistanceEnabled` flag is disabled.
  test(
      destination_item_test_cros.TestNames
          .PrinterIconMapsToPrinterStatus_FlagOff,
      async function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        // Before destination status request icon should be grey.
        assertEquals('print-preview:printer-status-grey', icon.icon);

        // Verify destination with `PrinterStatusReason.NO_ERROR` uses a green
        // icon.
        listItem.destination = createTestDestination('One');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-green', icon.icon);

        // Verify destination with PrinterStatusReason that is not `NO_ERROR`,
        // null, or `UNKNOWN_REASON` uses a red icon.
        listItem.destination = createTestDestination('Two');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-red', icon.icon);

        // Verify destination with `PrinterStatusReason.PRINTER_UNREACHABLE`
        // uses a red icon.
        listItem.destination = createTestDestination('Three');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-red', icon.icon);

        // Verify destination with `PrinterStatusReason.UNKNOWN_REASON` uses a
        // grey icon.
        listItem.destination = createTestDestination('Four');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-grey', icon.icon);
      });

  // Verifies expected icon displays for given status when
  // `isPrintPreviewSetupAssistanceEnabled` flag is enabled.
  test(
      destination_item_test_cros.TestNames
          .PrinterIconMapsToPrinterStatus_FlagOn,
      async function() {
        // Set flag on and reset destination item to ensure it is using the
        // latest flag state.
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });

        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        // Before destination status request icon should be grey.
        assertEquals('print-preview:printer-status-grey', icon.icon);

        // Verify destination with `PrinterStatusReason.NO_ERROR` uses a green
        // icon.
        listItem.destination = createTestDestination('One');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-green', icon.icon);

        // Verify destination with PrinterStatusReason that is not
        // `PRINTER_UNREACHABLE`, `NO_ERROR`, or unknown uses a orange icon.
        listItem.destination = createTestDestination('Two');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-orange', icon.icon);

        // Verify destination with `PrinterStatusReason.PRINTER_UNREACHABLE`
        // uses a red icon.
        listItem.destination = createTestDestination('Three');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-red', icon.icon);

        // Verify destination with `PrinterStatusReason.UNKNOWN_REASON` uses a
        // green icon.
        listItem.destination = createTestDestination('Four');
        await listItem.destination.requestPrinterStatus();
        assertEquals('print-preview:printer-status-green', icon.icon);
      });

  // Verifies expected hidden state and class applied to status text depending
  // on PrinterStatusReason when `isPrintPreviewSetupAssistanceEnabled` flag is
  // disabled.
  test(
      destination_item_test_cros.TestNames.PrinterConnectionStatusClass_FlagOff,
      async function() {
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: false,
        });
        const connectionText: HTMLElement =
            listItem.shadowRoot!.querySelector<HTMLElement>(
                '.connection-status')!;
        // Before destination status is empty and connection text is hidden.
        assertTrue(connectionText.hidden);

        // Verify destination with `PrinterStatusReason.NO_ERROR` connection
        // text hidden.
        listItem.destination = createTestDestination('One');
        await listItem.destination.requestPrinterStatus();
        assertTrue(connectionText.hidden);

        // Verify destination with PrinterStatusReason that is not `NO_ERROR`,
        // null, or `UNKNOWN_REASON` uses a red connection text.
        listItem.destination = createTestDestination('Two');
        await listItem.destination.requestPrinterStatus();
        assertFalse(connectionText.hidden);
        const expectedClassName = 'connection-status';
        assertEquals(expectedClassName, connectionText.className.trim());

        // Verify destination with `PrinterStatusReason.PRINTER_UNREACHABLE`
        // uses a red connection text.
        listItem.destination = createTestDestination('Three');
        await listItem.destination.requestPrinterStatus();
        assertFalse(connectionText.hidden);
        assertEquals(expectedClassName, connectionText.className.trim());

        // Verify destination with `PrinterStatusReason.UNKNOWN_REASON`
        // connection text is hidden.
        listItem.destination = createTestDestination('Four');
        await listItem.destination.requestPrinterStatus();
        assertTrue(connectionText.hidden);
      });

  // Verifies expected hidden state and class applied to status text depending
  // on PrinterStatusReason when `isPrintPreviewSetupAssistanceEnabled` flag is
  // disabled.
  test(
      destination_item_test_cros.TestNames.PrinterConnectionStatusClass_FlagOn,
      async function() {
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });
        const connectionText: HTMLElement =
            listItem.shadowRoot!.querySelector<HTMLElement>(
                '.connection-status')!;
        // Before destination status is empty and connection text is hidden.
        assertTrue(connectionText.hidden);

        // Verify destination with `PrinterStatusReason.NO_ERROR` connection
        // text hidden.
        listItem.destination = createTestDestination('One');
        await listItem.destination.requestPrinterStatus();
        assertTrue(connectionText.hidden);

        // Verify destination with PrinterStatusReason that is not `NO_ERROR`,
        // null, or `UNKNOWN_REASON` uses orange connection text.
        listItem.destination = createTestDestination('Two');
        await listItem.destination.requestPrinterStatus();
        assertFalse(connectionText.hidden);
        assertTrue(connectionText.classList.contains('status-orange'));

        // Verify destination with `PrinterStatusReason.PRINTER_UNREACHABLE`
        // uses red connection text.
        listItem.destination = createTestDestination('Three');
        await listItem.destination.requestPrinterStatus();
        assertFalse(connectionText.hidden);
        assertTrue(connectionText.classList.contains('status-red'));

        // Verify destination with `PrinterStatusReason.UNKNOWN_REASON`
        // connections text is hidden.
        listItem.destination = createTestDestination('Four');
        await listItem.destination.requestPrinterStatus();
        assertTrue(connectionText.hidden);
      });
});
