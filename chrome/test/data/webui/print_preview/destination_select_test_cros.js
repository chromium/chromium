// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, getSelectDropdownBackground, NativeLayer, NativeLayerImpl, PrinterStatus, PrinterStatusReason, PrinterStatusSeverity, SAVE_TO_DRIVE_CROS_DESTINATION_KEY} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Base, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getGoogleDriveDestination, getSaveAsPdfDestination, selectOption} from './print_preview_test_utils.js';

window.printer_status_test_cros = {};
const printer_status_test_cros = window.printer_status_test_cros;
printer_status_test_cros.suiteName = 'PrinterStatusTestCros';
/** @enum {string} */
printer_status_test_cros.TestNames = {
  PrinterStatusUpdatesColor: 'printer status updates color',
  SendStatusRequestOnce: 'send status request once',
  HiddenStatusText: 'hidden status text',
  ChangeIcon: 'change icon',
};

suite(printer_status_test_cros.suiteName, function() {
  /** @type {!PrintPreviewDestinationSelectCrosElement} */
  let destinationSelect;

  const account = 'foo@chromium.org';

  /** @type {?NativeLayerStub} */
  let nativeLayer = null;

  function setNativeLayerPrinterStatusMap() {
    [
     {
       printerId: 'ID1',
       statusReasons: [{
         reason: PrinterStatusReason.NO_ERROR,
         severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
       }],
     },
     {
       printerId: 'ID2',
       statusReasons: [
         {
           reason: PrinterStatusReason.NO_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         },
         {
           reason: PrinterStatusReason.LOW_ON_PAPER,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         }
       ],
     },
     {
       printerId: 'ID3',
       statusReasons: [
         {
           reason: PrinterStatusReason.NO_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         },
         {
           reason: PrinterStatusReason.LOW_ON_PAPER,
           severity: PrinterStatusSeverity.REPORT
         }
       ],
     },
     {
       printerId: 'ID4',
       statusReasons: [
         {
           reason: PrinterStatusReason.NO_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         },
         {
           reason: PrinterStatusReason.LOW_ON_PAPER,
           severity: PrinterStatusSeverity.WARNING
         }
       ],
     },
     {
       printerId: 'ID5',
       statusReasons: [
         {
           reason: PrinterStatusReason.NO_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         },
         {
           reason: PrinterStatusReason.LOW_ON_PAPER,
           severity: PrinterStatusSeverity.ERROR
         }
       ],
     },
     {
       printerId: 'ID6',
       statusReasons: [
         {
           reason: PrinterStatusReason.DEVICE_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         },
         {
           reason: PrinterStatusReason.PRINTER_QUEUE_FULL,
           severity: PrinterStatusSeverity.ERROR
         }
       ],
     },
     {
       printerId: 'ID7',
       statusReasons: [
         {
           reason: PrinterStatusReason.DEVICE_ERROR,
           severity: PrinterStatusSeverity.REPORT
         },
         {
           reason: PrinterStatusReason.PRINTER_QUEUE_FULL,
           severity: PrinterStatusSeverity.REPORT
         }
       ],
     }].forEach(status =>
                  nativeLayer.addPrinterStatusToMap(status.printerId, status));
  }

  /**
   * @param {string} id
   * @param {string} displayName
   * @param {!DestinationOrigin} destinationOrigin
   * @return {!Destination}
   */
  function createDestination(id, displayName, destinationOrigin) {
    return new Destination(
        id, DestinationType.LOCAL, destinationOrigin, displayName,
        DestinationConnectionStatus.ONLINE);
  }

  /**
   * @param {string} value
   * @return {string}
   */
  function escapeForwardSlahes(value) {
    return value.replace(/\//g, '\\/');
  }

  setup(function() {
    document.body.innerHTML = '';

    // Stub out native layer.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    setNativeLayerPrinterStatusMap();

    destinationSelect =
        /** @type {!PrintPreviewDestinationSelectCrosElement} */
        (document.createElement('print-preview-destination-select-cros'));
    destinationSelect.statusRequestedMap = new Map();
    document.body.appendChild(destinationSelect);
  });

  test(
      assert(printer_status_test_cros.TestNames.PrinterStatusUpdatesColor),
      function() {
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

        return waitBeforeNextRender(destinationSelect)
            .then(() => {
              const whenStatusRequestsDone =
                  nativeLayer.waitForMultiplePrinterStatusRequests(7);

              destinationSelect.recentDestinationList = [
                destination1,
                destination2,
                destination3,
                destination4,
                destination5,
                destination6,
                destination7,
              ];

              return whenStatusRequestsDone;
            })
            .then(() => {
              return waitBeforeNextRender(destinationSelect);
            })
            .then(() => {
              const dropdown = destinationSelect.$$('#dropdown');
              assertEquals(
                  'print-preview:printer-status-green',
                  dropdown.$$(`#${escapeForwardSlahes(destination1.key)}`)
                      .firstChild.icon);
              assertEquals(
                  'print-preview:printer-status-green',
                  dropdown.$$(`#${escapeForwardSlahes(destination2.key)}`)
                      .firstChild.icon);
              assertEquals(
                  'print-preview:printer-status-green',
                  dropdown.$$(`#${escapeForwardSlahes(destination3.key)}`)
                      .firstChild.icon);
              assertEquals(
                  'print-preview:printer-status-red',
                  dropdown.$$(`#${escapeForwardSlahes(destination4.key)}`)
                      .firstChild.icon);
              assertEquals(
                  'print-preview:printer-status-red',
                  dropdown.$$(`#${escapeForwardSlahes(destination5.key)}`)
                      .firstChild.icon);
              assertEquals(
                  'print-preview:printer-status-red',
                  dropdown.$$(`#${escapeForwardSlahes(destination6.key)}`)
                      .firstChild.icon);
              assertEquals(
                  'print-preview:printer-status-grey',
                  dropdown.$$(`#${escapeForwardSlahes(destination7.key)}`)
                      .firstChild.icon);
            });
        });

  test(
      assert(printer_status_test_cros.TestNames.SendStatusRequestOnce),
      function() {
        return waitBeforeNextRender(destinationSelect).then(() => {
          const destination1 =
              createDestination('ID1', 'One', DestinationOrigin.CROS);
          const destination2 =
              createDestination('ID2', 'Two', DestinationOrigin.CROS);

          destinationSelect.recentDestinationList = [
            destination1,
            destination2,
            createDestination('ID3', 'Three', DestinationOrigin.PRIVET),
            createDestination('ID4', 'Four', DestinationOrigin.EXTENSION),
          ];
          assertEquals(
              2, nativeLayer.getCallCount('requestPrinterStatusUpdate'));

          // Update list with 2 existing destinations and one new destination.
          // Make sure the requestPrinterStatusUpdate only gets called for the
          // new destination.
          destinationSelect.recentDestinationList = [
            destination1,
            destination2,
            createDestination('ID5', 'Five', DestinationOrigin.CROS),
          ];
          assertEquals(
              3, nativeLayer.getCallCount('requestPrinterStatusUpdate'));
        });
      });

  test(assert(printer_status_test_cros.TestNames.HiddenStatusText), function() {
    const destinationStatus =
        destinationSelect.$$('.destination-additional-info');
    return waitBeforeNextRender(destinationSelect)
        .then(() => {
          const destinationWithoutErrorStatus =
              createDestination('ID1', 'One', DestinationOrigin.CROS);
          // Destination with ID4 will return an error printer status that will
          // trigger the error text being populated.
          const destinationWithErrorStatus =
              createDestination('ID4', 'Four', DestinationOrigin.CROS);
          const cloudPrintDestination = new Destination(
              'ID2', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Two',
              DestinationConnectionStatus.OFFLINE, {account: account});

          destinationSelect.recentDestinationList = [
            destinationWithoutErrorStatus,
            destinationWithErrorStatus,
            cloudPrintDestination,
          ];

          const destinationEulaWrapper =
              destinationSelect.$$('#destinationEulaWrapper');

          destinationSelect.destination = cloudPrintDestination;
          assertFalse(destinationStatus.hidden);
          assertTrue(destinationEulaWrapper.hidden);

          destinationSelect.destination = destinationWithoutErrorStatus;
          assertTrue(destinationStatus.hidden);
          assertTrue(destinationEulaWrapper.hidden);

          destinationSelect.set(
              'destination.eulaUrl', 'chrome://os-credits/eula');
          assertFalse(destinationEulaWrapper.hidden);

          destinationSelect.destination = destinationWithErrorStatus;
          return nativeLayer.whenCalled('requestPrinterStatusUpdate');
        })
        .then(() => {
          return waitBeforeNextRender(destinationSelect);
        })
        .then(() => {
          assertFalse(destinationStatus.hidden);
        });
  });

  test(assert(printer_status_test_cros.TestNames.ChangeIcon), function() {
    return waitBeforeNextRender(destinationSelect).then(() => {
      const localCrosPrinter =
          createDestination('ID1', 'One', DestinationOrigin.CROS);
      const localNonCrosPrinter =
          createDestination('ID2', 'Two', DestinationOrigin.LOCAL);
      const saveToDrive = getGoogleDriveDestination('account');
      const saveAsPdf = getSaveAsPdfDestination();

      destinationSelect.recentDestinationList = [
        localCrosPrinter,
        saveToDrive,
        saveAsPdf,
      ];
      const dropdown = destinationSelect.$$('#dropdown');

      destinationSelect.destination = localCrosPrinter;
      destinationSelect.updateDestination();
      assertEquals(
          'print-preview:printer-status-grey', dropdown.destinationIcon);

      destinationSelect.destination = localNonCrosPrinter;
      destinationSelect.updateDestination();
      assertEquals('print-preview:print', dropdown.destinationIcon);

      destinationSelect.destination = saveToDrive;
      destinationSelect.updateDestination();
      assertEquals('print-preview:save-to-drive', dropdown.destinationIcon);

      destinationSelect.destination = saveAsPdf;
      destinationSelect.updateDestination();
      assertEquals('cr:insert-drive-file', dropdown.destinationIcon);
    });
  });
});

window.destination_select_test_cros = {};
const destination_select_test_cros = window.destination_select_test_cros;
destination_select_test_cros.suiteName = 'DestinationSelectTestCros';
/** @enum {string} */
destination_select_test_cros.TestNames = {
  UpdateStatus: 'update status',
  UpdateStatusDeprecationWarnings: 'update status deprecation warnings',
  ChangeIcon: 'change icon',
  ChangeIconDeprecationWarnings: 'change icon deprecation warnings',
  EulaIsDisplayed: 'eula is displayed',
};

suite(destination_select_test_cros.suiteName, function() {
  /** @type {!PrintPreviewDestinationSelectCrosElement} */
  let destinationSelect;

  /** @type {string} */
  const account = 'foo@chromium.org';

  /** @type {!DestinationOrigin} */
  const cookieOrigin = DestinationOrigin.COOKIES;

  /** @type {string} */
  const driveKey =
      `${Destination.GooglePromotedId.DOCS}/${cookieOrigin}/${account}`;

  /** @type {!Array<!Destination>} */
  let recentDestinationList = [];

  const meta = /** @type {!IronMetaElement} */ (
      Base.create('iron-meta', {type: 'iconset'}));

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    destinationSelect =
        /** @type {!PrintPreviewDestinationSelectCrosElement} */
        (document.createElement('print-preview-destination-select-cros'));
    destinationSelect.activeUser = account;
    destinationSelect.appKioskMode = false;
    destinationSelect.disabled = false;
    destinationSelect.loaded = false;
    destinationSelect.noDestinations = false;
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    document.body.appendChild(destinationSelect);
  });

  // Create three different destinations and use them to populate
  // |recentDestinationList|.
  function populateRecentDestinationList() {
    recentDestinationList = [
      new Destination(
          'ID1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
          DestinationConnectionStatus.ONLINE),
      new Destination(
          'ID2', DestinationType.GOOGLE, cookieOrigin, 'Two',
          DestinationConnectionStatus.OFFLINE, {account: account}),
      new Destination(
          'ID3', DestinationType.GOOGLE, cookieOrigin, 'Three',
          DestinationConnectionStatus.ONLINE,
          {account: account, isOwned: true}),
    ];
  }

  function compareIcon(selectEl, expectedIcon) {
    const icon = selectEl.style['background-image'].replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        /** @type {!IronIconsetSvgElement} */
        (meta.byKey('print-preview')), expectedIcon, destinationSelect);
    assertEquals(expected, icon);
  }

  /**
   * Test that changing different destinations results in the correct icon being
   * shown.
   * @param {boolean} cloudPrintDeprecationWarningsSuppressed Whether cloud
   *     print deprecation warnings should be suppressed.
   * @return {!Promise} Promise that resolves when the test finishes.
   */
  function testChangeIcon(cloudPrintDeprecationWarningsSuppressed) {
    let selectEl;

    return waitBeforeNextRender(destinationSelect)
        .then(() => {
          const destination = recentDestinationList[0];
          destinationSelect.destination = destination;
          destinationSelect.updateDestination();
          destinationSelect.loaded = true;
          selectEl = destinationSelect.$$('.md-select');
          compareIcon(selectEl, 'print');
          destinationSelect.driveDestinationKey = driveKey;

          return selectOption(
              destinationSelect,
                  driveKey);
        })
        .then(() => {
          // Icon updates early based on the ID.
          compareIcon(selectEl, 'save-to-drive');

          // Update the destination.
          destinationSelect.destination = getGoogleDriveDestination(account);

          // Still Save to Drive icon.
          compareIcon(selectEl, 'save-to-drive');

          // Select a destination with the shared printer icon.
          return selectOption(
              destinationSelect, `ID2/${cookieOrigin}/${account}`);
        })
        .then(() => {
          const dest2Icon = cloudPrintDeprecationWarningsSuppressed ?
              'printer-shared' :
              'printer-not-supported';

          // Should already be updated.
          compareIcon(selectEl, dest2Icon);

          // Update destination.
          destinationSelect.destination = recentDestinationList[1];
          compareIcon(selectEl, dest2Icon);

          // Select a destination with a standard printer icon.
          return selectOption(
              destinationSelect, `ID3/${cookieOrigin}/${account}`);
        })
        .then(() => {
          const dest3Icon = cloudPrintDeprecationWarningsSuppressed ?
              'print' :
              'printer-not-supported';

          compareIcon(selectEl, dest3Icon);

          // Update destination.
          destinationSelect.destination = recentDestinationList[2];
          compareIcon(selectEl, dest3Icon);
        });
  }

  /**
   * Test that changing different destinations results in the correct status
   * being shown.
   * @param {boolean} cloudPrintDeprecationWarningsSuppressed Whether cloud
   *     print deprecation warnings should be suppressed.
   */
  function testUpdateStatus(cloudPrintDeprecationWarningsSuppressed) {
    loadTimeData.overrideValues({
      offline: 'offline',
      printerNotSupportedWarning: 'printerNotSupportedWarning',
    });

    assertFalse(destinationSelect.$$('.throbber-container').hidden);
    assertTrue(destinationSelect.$$('.md-select').hidden);

    destinationSelect.loaded = true;
    assertTrue(destinationSelect.$$('.throbber-container').hidden);
    assertFalse(destinationSelect.$$('.md-select').hidden);

    const additionalInfoEl =
        destinationSelect.$$('.destination-additional-info');
    const statusEl = destinationSelect.$$('#statusText');

    destinationSelect.driveDestinationKey = driveKey;
    destinationSelect.destination = getGoogleDriveDestination(account);
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[0];
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[1];
    destinationSelect.updateDestination();
    assertFalse(additionalInfoEl.hidden);
    assertEquals('offline', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[2];
    destinationSelect.updateDestination();
    assertEquals(
        cloudPrintDeprecationWarningsSuppressed, additionalInfoEl.hidden);
    const dest3Status = cloudPrintDeprecationWarningsSuppressed ?
        '' :
        'printerNotSupportedWarning';
    assertEquals(dest3Status, statusEl.innerHTML);
  }

  test(assert(destination_select_test_cros.TestNames.UpdateStatus), function() {
    loadTimeData.overrideValues(
        {cloudPrintDeprecationWarningsSuppressed: true});

    // Repopulate |recentDestinationList| to have
    // |cloudPrintDeprecationWarningsSuppressed| take effect during creation of
    // new Destinations.
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    return waitBeforeNextRender(destinationSelect).then(() => {
      testUpdateStatus(true);
    });
  });

  test(
      assert(destination_select_test_cros.TestNames
                 .UpdateStatusDeprecationWarnings),
      function() {
        return waitBeforeNextRender(destinationSelect).then(() => {
          testUpdateStatus(false);
        });
      });

  test(assert(destination_select_test_cros.TestNames.ChangeIcon), function() {
    loadTimeData.overrideValues(
        {cloudPrintDeprecationWarningsSuppressed: true});

    // Repopulate |recentDestinationList| to have
    // |cloudPrintDeprecationWarningsSuppressed| take effect during creation of
    // new Destinations.
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    return testChangeIcon(true);
  });

  test(
      assert(
          destination_select_test_cros.TestNames.ChangeIconDeprecationWarnings),
      function() {
        return testChangeIcon(false);
      });

  /**
   * Tests that destinations with a EULA will display the EULA URL.
   */
  test(
      assert(destination_select_test_cros.TestNames.EulaIsDisplayed),
      function() {
        destinationSelect.destination = recentDestinationList[0];
        destinationSelect.loaded = true;
        const destinationEulaWrapper =
            destinationSelect.$$('#destinationEulaWrapper');
        assertTrue(destinationEulaWrapper.hidden);

        destinationSelect.set(
            'destination.eulaUrl', 'chrome://os-credits/eula');
        assertFalse(destinationEulaWrapper.hidden);
      });
});
