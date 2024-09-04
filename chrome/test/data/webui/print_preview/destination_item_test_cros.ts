// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, getTrustedHTML, NativeLayerCrosImpl, PrinterStatusReason, PrinterStatusSeverity} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {FakeMediaQueryList} from './print_preview_test_utils.js';

suite('DestinationItemTestCros', function() {
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
      'NewStatusUpdatesIcon', function() {
        const icon = listItem.shadowRoot!.querySelector('cr-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);

        return listItem.destination.requestPrinterStatus().then(() => {
          assertEquals('print-preview:printer-status-green', icon.icon);
        });
      });

  test(
      'ChangingDestinationUpdatesIcon', function() {
        const icon = listItem.shadowRoot!.querySelector('cr-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);

        listItem.destination = new Destination(
            'Two', DestinationOrigin.CROS, 'Destination Two',
            {description: 'ABC'});

        return waitBeforeNextRender(listItem).then(() => {
          assertEquals('print-preview:printer-status-orange', icon.icon);
        });
      });

  // Tests that the printer stauts icon is only notified to update if the
  // destination key in the printer status response matches the current
  // destination.
  test(
      'OnlyUpdateMatchingDestination', function() {
        const icon = listItem.shadowRoot!.querySelector('cr-icon')!;
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

  // Verifies expected icon displays for given status.
  test('PrinterIconMapsToPrinterStatus', async function() {
    const icon = listItem.shadowRoot!.querySelector('cr-icon')!;
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
  // on PrinterStatusReason.
  test('PrinterConnectionStatusClass', async function() {
    const connectionText: HTMLElement =
        listItem.shadowRoot!.querySelector<HTMLElement>('.connection-status')!;
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
