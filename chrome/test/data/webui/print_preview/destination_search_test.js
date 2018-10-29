// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_search_test', function() {
  /** @enum {string} */
  const TestNames = {
    ReceiveSuccessfulSetup: 'receive successful setup',
    ResolutionFails: 'resolution fails',
    ReceiveFailedSetup: 'receive failed setup',
    GetCapabilitiesFails: 'get capabilities fails',
    CloudKioskPrinter: 'cloud kiosk printer',
    ReceiveSuccessfulSetupWithPolicies:
        'receive successful setup with policies',
  };

  const suiteName = 'NewDestinationSearchTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationDialogElement} */
    let dialog = null;

    /** @type {?print_preview.DestinationStore} */
    let destinationStore = null;

    /** @type {?print_preview.UserInfo} */
    let userInfo = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @override */
    setup(function() {
      // Create data classes
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      userInfo = new print_preview.UserInfo();
      destinationStore = new print_preview.DestinationStore(
          userInfo, new WebUIListenerTracker());
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate('FooDevice', 'FooName'));
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          [] /* recentDestinations */);

      // Set up dialog
      dialog = document.createElement('print-preview-destination-dialog');
      dialog.userInfo = userInfo;
      dialog.destinationStore = destinationStore;
      dialog.invitationStore = new print_preview.InvitationStore(userInfo);
      dialog.recentDestinations = [];
      PolymerTest.clearBody();
      document.body.appendChild(dialog);
      return nativeLayer.whenCalled('getPrinterCapabilities').then(function() {
        dialog.show();
        Polymer.dom.flush();
        nativeLayer.reset();
      });
    });

    /**
     * @param {!print_preview.Destination} destination The destination to
     *     simulate selection of.
     */
    function simulateDestinationSelect(destination) {
      // Fake destinationListItem.
      const item =
          document.createElement('print-preview-destination-list-item');
      item.destination = destination;

      // Get print list and fire event.
      const list = dialog.shadowRoot.querySelectorAll(
          'print-preview-destination-list')[1];
      list.fire('destination-selected', item);
    }

    /**
     * Adds a destination to the dialog and simulates selection of the
     * destination.
     * @param {string} destId The ID for the destination.
     */
    function requestSetup(destId) {
      const origin = cr.isChromeOS ? print_preview.DestinationOrigin.CROS :
                                     print_preview.DestinationOrigin.LOCAL;

      const dest = new print_preview.Destination(
          destId, print_preview.DestinationType.LOCAL, origin, 'displayName',
          print_preview.DestinationConnectionStatus.ONLINE);

      // Add the destination to the list.
      dialog.updateDestinations_([dest]);
      simulateDestinationSelect(dest);
    }

    // Tests that a destination is selected if the user clicks on it and setup
    // (for CrOS) or capabilities fetch (for non-Cros) succeeds.
    test(assert(TestNames.ReceiveSuccessfulSetup), function() {
      const destId = '00112233DEADBEEF';
      const response = {
        printerId: destId,
        capabilities:
            print_preview_test_utils.getCddTemplate(destId).capabilities,
        success: true,
      };
      if (cr.isChromeOS) {
        nativeLayer.setSetupPrinterResponse(response);
      } else {
        nativeLayer.setLocalDestinationCapabilities(
            print_preview_test_utils.getCddTemplate(destId));
      }

      const waiter = test_util.eventToPromise(
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          destinationStore);
      requestSetup(destId);
      const callback =
          cr.isChromeOS ? 'setupPrinter' : 'getPrinterCapabilities';
      return Promise.all([nativeLayer.whenCalled(callback), waiter])
          .then(function(results) {
            const actualId =
                cr.isChromeOS ? results[0] : results[0].destinationId;
            assertEquals(destId, actualId);
            // After setup or capabilities fetch succeeds, the destination
            // should be selected.
            assertNotEquals(null, destinationStore.selectedDestination);
            assertEquals(destId, destinationStore.selectedDestination.id);
          });
    });

    // Test what happens when the setupPrinter request is rejected. ChromeOS
    // only.
    test(assert(TestNames.ResolutionFails), function() {
      const destId = '001122DEADBEEF';
      const originalDestination = destinationStore.selectedDestination;
      nativeLayer.setSetupPrinterResponse(
          {printerId: destId, success: false}, true);
      requestSetup(destId);
      return nativeLayer.whenCalled('setupPrinter').then(function(actualId) {
        assertEquals(destId, actualId);
        // The selected printer should not have changed, since a printer
        // cannot be selected until setup succeeds.
        assertEquals(originalDestination, destinationStore.selectedDestination);
      });
    });

    // Test what happens when the setupPrinter request is resolved with a
    // failed status. Chrome OS only.
    test(assert(TestNames.ReceiveFailedSetup), function() {
      const originalDestination = destinationStore.selectedDestination;
      const destId = '00112233DEADBEEF';
      const response = {
        printerId: destId,
        capabilities:
            print_preview_test_utils.getCddTemplate(destId).capabilities,
        success: false,
      };
      nativeLayer.setSetupPrinterResponse(response);
      requestSetup(destId);
      return nativeLayer.whenCalled('setupPrinter')
          .then(function(actualDestId) {
            assertEquals(destId, actualDestId);
            // The selected printer should not have changed, since a printer
            // cannot be selected until setup succeeds.
            assertEquals(
                originalDestination, destinationStore.selectedDestination);
          });
    });

    // Tests what happens when capabilities cannot be retrieved for the chosen
    // destination. The destination will still be selected in this case.
    // non-Chrome OS only.
    test(assert(TestNames.GetCapabilitiesFails), function() {
      const destId = '001122DEADBEEF';
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(destId), true);
      requestSetup(destId);
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(function(args) {
            assertEquals(destId, args.destinationId);
            // The destination is selected even though capabilities cannot be
            // retrieved.
            assertEquals(destId, destinationStore.selectedDestination.id);
          });
    });

    // Test what happens when a simulated cloud kiosk printer is selected.
    test(assert(TestNames.CloudKioskPrinter), function() {
      const printerId = 'cloud-printer-id';

      // Create cloud destination.
      const cloudDest = new print_preview.Destination(
          printerId, print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.DEVICE, 'displayName',
          print_preview.DestinationConnectionStatus.ONLINE);
      cloudDest.capabilities =
          print_preview_test_utils.getCddTemplate(printerId, 'displayName')
              .capabilities;

      // Place destination in the local list as happens for Kiosk printers.
      dialog.updateDestinations_([cloudDest]);
      simulateDestinationSelect(cloudDest);

      // Verify that the destination has been selected.
      assertEquals(printerId, destinationStore.selectedDestination.id);
    });

    // Tests that if policies are set correctly if they are present
    // for a destination. ChromeOS only.
    test(assert(TestNames.ReceiveSuccessfulSetupWithPolicies), function() {
      const destId = '00112233DEADBEEF';
      const response = {
        printerId: destId,
        capabilities:
            print_preview_test_utils.getCddTemplate(destId).capabilities,
        policies: {
          allowedColorModes: print_preview.ColorMode.GRAY,
          allowedDuplexModes: print_preview.DuplexModeRestriction.DUPLEX,
        },
        success: true,
      };
      nativeLayer.setSetupPrinterResponse(response);
      requestSetup(destId);
      return nativeLayer.whenCalled('setupPrinter').then(function(actualId) {
        assertEquals(destId, actualId);
        const selectedDestination = destinationStore.selectedDestination;
        assertNotEquals(null, selectedDestination);
        assertEquals(destId, selectedDestination.id);
        assertNotEquals(null, selectedDestination.capabilities);
        assertNotEquals(null, selectedDestination.policies);
        assertEquals(
            print_preview.ColorMode.GRAY,
            selectedDestination.policies.allowedColorModes);
        assertEquals(
            print_preview.DuplexModeRestriction.DUPLEX,
            selectedDestination.policies.allowedDuplexModes);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
