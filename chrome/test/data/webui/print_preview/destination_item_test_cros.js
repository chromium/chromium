// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, NativeLayer, NativeLayerImpl, PrinterState, PrinterStatusReason, PrinterStatusSeverity} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

import {NativeLayerStub} from './native_layer_stub.js';

window.destination_item_test_cros = {};
const destination_item_test_cros = window.destination_item_test_cros;
destination_item_test_cros.suiteName = 'DestinationItemTestCros';
/** @enum {string} */
destination_item_test_cros.TestNames = {
  NewStatusUpdatesIcon: 'new status updates icon',
  ChangingDestinationUpdatesIcon: 'changing destination updates icon',
  OnlyUpdateMatchingDestination: 'only update matching destination',
};

suite(destination_item_test_cros.suiteName, function() {
  /** @type {!PrintPreviewDestinationListItemElement} */
  let listItem;

  /** @type {?NativeLayerStub} */
  let nativeLayer = null;

  function setNativeLayerPrinterStatusMap() {
    [{
      printerId: 'One',
      statusReasons: [{
        reason: PrinterStatusReason.NO_ERROR,
        severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
      }],
    },
     {
       printerId: 'Two',
       statusReasons: [{
         reason: PrinterStatusReason.OUT_OF_INK,
         severity: PrinterStatusSeverity.ERROR
       }],
     }]
        .forEach(
            status =>
                nativeLayer.addPrinterStatusToMap(status.printerId, status));
  }

  /** @override */
  setup(function() {
    document.body.innerHTML = `
          <print-preview-destination-list-item id="listItem">
          </print-preview-destination-list-item>`;

    // Stub out native layer.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    setNativeLayerPrinterStatusMap();

    listItem = /** @type {!PrintPreviewDestinationListItemElement} */ (
        document.body.querySelector('#listItem'));
    listItem.destination = new Destination(
        'One', DestinationType.LOCAL, DestinationOrigin.CROS, 'Destination One',
        DestinationConnectionStatus.ONLINE, {description: 'ABC'});
    flush();
  });

  test(
      assert(destination_item_test_cros.TestNames.NewStatusUpdatesIcon),
      function() {
        const icon = listItem.$$('iron-icon');
        assertEquals('print-preview:printer-status-grey', icon.icon);

        return listItem.destination.requestPrinterStatus().then(() => {
          assertEquals('print-preview:printer-status-green', icon.icon);
        });
      });

  test(
      assert(
          destination_item_test_cros.TestNames.ChangingDestinationUpdatesIcon),
      function() {
        const icon = listItem.$$('iron-icon');
        assertEquals('print-preview:printer-status-grey', icon.icon);

        listItem.destination = new Destination(
            'Two', DestinationType.LOCAL, DestinationOrigin.CROS,
            'Destination Two', DestinationConnectionStatus.ONLINE,
            {description: 'ABC'});

        return waitBeforeNextRender(listItem).then(() => {
          assertEquals('print-preview:printer-status-red', icon.icon);
        });
      });

  // Tests that the printer stauts icon is only notified to update if the
  // destination key in the printer status response matches the current
  // destination.
  test(
      assert(
          destination_item_test_cros.TestNames.OnlyUpdateMatchingDestination),
      function() {
        const icon = listItem.$$('iron-icon');
        assertEquals('print-preview:printer-status-grey', icon.icon);
        const firstDestinationStatusRequestPromise =
            listItem.destination.requestPrinterStatus();

        // Simulate destination_list updating and switching the destination
        // after the request for the original destination was already sent out.
        listItem.destination = new Destination(
            'Two', DestinationType.LOCAL, DestinationOrigin.CROS,
            'Destination Two', DestinationConnectionStatus.ONLINE,
            {description: 'ABC'});

        return firstDestinationStatusRequestPromise.then(() => {
          // PrinterState should stay the same because the destination in the
          // status request response doesn't match.
          assertEquals('print-preview:printer-status-grey', icon.icon);
        });
      });
});
