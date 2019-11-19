// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Helper function that waits for |getEulaUrl| to get called and then verifies
 * its arguments.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {string} expectedManufacturer
 * @param {string} expectedModel
 * @return {!Promise}
 */
function verifyGetEulaUrlWasCalled(
    cupsPrintersBrowserProxy, expectedManufacturer, expectedModel) {
  return cupsPrintersBrowserProxy.whenCalled('getEulaUrl').then(function(args) {
    assertEquals(expectedManufacturer, args[0]);  // ppdManufacturer
    assertEquals(expectedModel, args[1]);         // ppdModel
  });
}

/*
 * Helper function that resets the resolver for |getEulaUrl| and sets the new
 * EULA URL.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {string} eulaUrl
 */
function resetGetEulaUrl(cupsPrintersBrowserProxy, eulaUrl) {
  cupsPrintersBrowserProxy.resetResolver('getEulaUrl');
  cupsPrintersBrowserProxy.setEulaUrl(eulaUrl);
}

suite('CupsAddPrinterDialogTests', function() {
  function fillAddManuallyDialog(addDialog) {
    const name = addDialog.$$('#printerNameInput');
    const address = addDialog.$$('#printerAddressInput');

    assertTrue(!!name);
    name.value = 'Test printer';

    assertTrue(!!address);
    address.value = '127.0.0.1';

    const addButton = addDialog.$$('#addPrinterButton');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
  }

  function clickAddButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for add');
    const addButton = dialog.$$('.action-button');
    assertTrue(!!addButton, 'Button is null');
    addButton.click();
  }

  function clickCancelButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton = dialog.$$('.cancel-button');
    assertTrue(!!cancelButton, 'Button is null');
    cancelButton.click();
  }

  function canAddPrinter(dialog, name, address) {
    dialog.newPrinter.printerName = name;
    dialog.newPrinter.printerAddress = address;
    return dialog.canAddPrinter_();
  }

  let page = null;
  let dialog = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  setup(function() {
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    dialog = page.$$('settings-cups-add-printer-dialog');
    assertTrue(!!dialog);

    dialog.open();
    Polymer.dom.flush();
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog = null;
    page = null;
  });

  /**
   * Test that the discovery dialog is showing when a user initially asks
   * to add a printer.
   */
  test('DiscoveryShowing', function() {
    return test_util.flushTasks().then(function() {
      // Discovery is showing.
      assertTrue(dialog.showDiscoveryDialog_);
      assertTrue(!!dialog.$$('add-printer-discovery-dialog'));

      // All other components are hidden.
      assertFalse(dialog.showManufacturerDialog_);
      assertFalse(!!dialog.$$('add-printer-manufacturer-model-dialog'));
      assertFalse(dialog.showConfiguringDialog_);
      assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
      assertFalse(dialog.showManuallyAddDialog_);
      assertFalse(!!dialog.$$('add-printer-manually-dialog'));
    });
  });

  test('ValidIPV4', function() {
    const dialog = document.createElement('add-printer-manually-dialog');
    expectTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1'));
  });

  test('ValidIPV4WithPort', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1:1234'));
  });

  test('ValidIPV6', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    // Test the full ipv6 address scheme.
    expectTrue(canAddPrinter(dialog, 'Test printer', '1:2:a3:ff4:5:6:7:8'));

    // Test the shorthand prefix scheme.
    expectTrue(canAddPrinter(dialog, 'Test printer', '::255'));

    // Test the shorthand suffix scheme.
    expectTrue(canAddPrinter(dialog, 'Test printer', '1::'));
  });

  test('ValidIPV6WithPort', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectTrue(canAddPrinter(dialog, 'Test printer', '[1:2:aa2:4]:12'));
    expectTrue(canAddPrinter(dialog, 'Test printer', '[::255]:54'));
    expectTrue(canAddPrinter(dialog, 'Test printer', '[1::]:7899'));
  });

  test('InvalidIPV6', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:4:5:6:7:8:9'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:aa:a1245:2'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:za:2'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1:::22'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1::2::3'));
  });

  test('ValidHostname', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectTrue(canAddPrinter(dialog, 'Test printer', 'hello-world.com'));
    expectTrue(canAddPrinter(dialog, 'Test printer', 'hello.world.com:12345'));
  });

  test('InvalidHostname', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectFalse(canAddPrinter(dialog, 'Test printer', 'helloworld!123.com'));
    expectFalse(canAddPrinter(dialog, 'Test printer', 'helloworld123-.com'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '-helloworld123.com'));
  });

  /**
   * Test that clicking on Add opens the model select page.
   */
  test('ValidAddOpensModelSelection', function() {
    // Starts in discovery dialog, select add manually button.
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$.manuallyAddPrinterButton.click();
    Polymer.dom.flush();

    // Now we should be in the manually add dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);

    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          return test_util.flushTasks();
        })
        .then(function() {
          // Showing model selection.
          assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
          assertTrue(!!dialog.$$('add-printer-manufacturer-model-dialog'));

          assertTrue(dialog.showManufacturerDialog_);
          assertFalse(dialog.showConfiguringDialog_);
          assertFalse(dialog.showManuallyAddDialog_);
          assertFalse(dialog.showDiscoveryDialog_);
        });
  });

  /**
   * Test that when getPrinterInfo fails for a generic reason, the general error
   * message is shown.
   */
  test('GetPrinterInfoFailsGeneralError', function() {
    // Starts in discovery dialog, select add manually button.
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$.manuallyAddPrinterButton.click();
    Polymer.dom.flush();

    // Now we should be in the manually add dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);

    fillAddManuallyDialog(addDialog);

    // Make the getPrinterInfo fail for a generic error.
    cupsPrintersBrowserProxy.setGetPrinterInfoResult(
        PrinterSetupResult.FATAL_ERROR);

    // Attempt to add the printer.
    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy.whenCalled('getPrinterInfo')
        .then(function(result) {
          // The general error should be showing.
          assertTrue(!!addDialog.errorText_);
          const generalErrorElement = addDialog.$$('printer-dialog-error');
          assertFalse(generalErrorElement.$$('#error-container').hidden);
        });
  });

  /**
   * Test that when getPrinterInfo fails for an unreachable printer, the printer
   * address field is marked as invalid.
   */
  test('GetPrinterInfoFailsUnreachableError', function() {
    // Starts in discovery dialog, select add manually button.
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$.manuallyAddPrinterButton.click();
    Polymer.dom.flush();

    // Now we should be in the manually add dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);

    fillAddManuallyDialog(addDialog);

    // Make the getPrinterInfo fail for an unreachable printer.
    cupsPrintersBrowserProxy.setGetPrinterInfoResult(
        PrinterSetupResult.PRINTER_UNREACHABLE);

    // Attempt to add the printer.
    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy.whenCalled('getPrinterInfo')
        .then(function(result) {
          // The printer address input should be marked as invalid.
          assertTrue(addDialog.$$('#printerAddressInput').invalid);
        });
  });


  /**
   * Test that getModels isn't called with a blank query.
   */
  test('NoBlankQueries', function() {
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$.manuallyAddPrinterButton.click();
    Polymer.dom.flush();

    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);

    // Verify that getCupsPrinterModelList is not called.
    cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList')
        .then(function(manufacturer) {
          assertNotReached(
              'No manufacturer was selected.  Unexpected model request.');
        });

    cupsPrintersBrowserProxy.manufacturers =
        ['ManufacturerA', 'ManufacturerB', 'Chromites'];
    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          const modelDialog =
              dialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!modelDialog);
          // Manufacturer dialog has been rendered and the model list was not
          // requested.  We're done.
        });
  });

  /**
   * Test that dialog cancellation is logged from the manufacturer screen for
   * IPP printers.
   */
  test('LogDialogCancelledIpp', function() {
    const makeAndModel = 'Printer Make And Model';
    // Start on add manually.
    dialog.fire('open-manually-add-printer-dialog');
    Polymer.dom.flush();

    // Populate the printer object.
    dialog.newPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '192.168.1.13',
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipps',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    // Seed the getPrinterInfo response.  We detect the make and model but it is
    // not an autoconf printer.
    cupsPrintersBrowserProxy.printerInfo = {
      autoconf: false,
      manufacturer: 'MFG',
      model: 'MDL',
      makeAndModel: makeAndModel,
    };

    // Press the add button to advance dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    clickAddButton(addDialog);

    // Click cancel on the manufacturer dialog when it shows up then verify
    // cancel was called with the appropriate parameters.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          Polymer.dom.flush();
          // Cancel setup with the cancel button.
          clickCancelButton(dialog.$$('add-printer-manufacturer-model-dialog'));
          return cupsPrintersBrowserProxy.whenCalled('cancelPrinterSetUp');
        })
        .then(function(printer) {
          assertTrue(!!printer, 'New printer is null');
          assertEquals(makeAndModel, printer.printerMakeAndModel);
        });
  });

  /**
   * Test that dialog cancellation is logged from the manufacturer screen for
   * USB printers.
   */
  test('LogDialogCancelledUSB', function() {
    const vendorId = 0x1234;
    const modelId = 0xDEAD;
    const manufacturer = 'PrinterMFG';
    const model = 'Printy Printerson';

    const usbInfo = {
      usbVendorId: vendorId,
      usbProductId: modelId,
      usbVendorName: manufacturer,
      usbProductName: model,
    };

    const expectedPrinter = 'PICK_ME!';

    const newPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'EEAADDAA',
      printerDescription: '',
      printerId: expectedPrinter,
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
      printerUsbInfo: usbInfo,
    };

    dialog.fire('open-discovery-printers-dialog');

    // Make 'addDiscoveredPrinter' fail so we get sent to the make/model dialog.
    cupsPrintersBrowserProxy.setAddDiscoveredPrinterFailure(newPrinter);

    return cupsPrintersBrowserProxy.whenCalled('startDiscoveringPrinters')
        .then(function() {
          // Select the printer.
          // TODO(skau): Figure out how to select in a dom-repeat.
          const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
          assertTrue(!!discoveryDialog, 'Cannot find discovery dialog');
          discoveryDialog.selectedPrinter = newPrinter;
          // Run printer setup.
          clickAddButton(discoveryDialog);
          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(function(printerId) {
          assertEquals(expectedPrinter, printerId);

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(function() {
          // Cancel setup with the cancel button.
          clickCancelButton(dialog.$$('add-printer-manufacturer-model-dialog'));
          return cupsPrintersBrowserProxy.whenCalled('cancelPrinterSetUp');
        })
        .then(function(printer) {
          assertEquals(expectedPrinter, printer.printerId);
          assertDeepEquals(usbInfo, printer.printerUsbInfo);
        });
  });

  /**
   * Test that the close button exists on the configure dialog.
   */
  test('ConfigureDialogCancelDisabled', function() {
    const newPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'EEAADDAA',
      printerDescription: '',
      printerId: 'printerId',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
      printerUsbInfo: '',
    };

    dialog.fire('open-discovery-printers-dialog');

    return cupsPrintersBrowserProxy.whenCalled('startDiscoveringPrinters')
        .then(function() {
          // Select the printer.
          const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
          assertTrue(!!discoveryDialog, 'Cannot find discovery dialog');
          discoveryDialog.selectedPrinter = newPrinter;
          // Run printer setup.
          clickAddButton(discoveryDialog);
          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(function(printerId) {
          const configureDialog = dialog.$$('add-printer-configuring-dialog');
          assertTrue(!!configureDialog);

          const closeButton = configureDialog.$$('.cancel-button');
          assertTrue(!!closeButton);
          assertFalse(closeButton.disabled);

          const waitForClose =
              test_util.eventToPromise('close', configureDialog);

          closeButton.click();
          Polymer.dom.flush();

          return waitForClose.then(() => {
            dialog = page.$$('settings-cups-add-printer-dialog');
            assertFalse(dialog.showConfiguringDialog_);
          });
        });
  });

  /**
   * Test that we are checking if a printer model has an EULA upon a model
   * change.
   */
  test('getEulaUrlGetsCalledOnModelChange', function() {
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$.manuallyAddPrinterButton.click();
    Polymer.dom.flush();

    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);

    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    const eulaLink = 'google.com';
    const expectedManufacturer = 'Google';
    const expectedModel = 'printer';
    const expectedModel2 = 'newPrinter';
    const expectedModel3 = 'newPrinter2';

    let modelDialog = null;
    let urlElement = null;
    let modelDropdown = null;

    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          modelDialog = dialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!modelDialog);

          urlElement = modelDialog.$$('#eulaUrl');
          // Check that the EULA text is not shown.
          assertTrue(urlElement.hidden);

          cupsPrintersBrowserProxy.setEulaUrl(eulaLink);

          modelDialog.$$('#manufacturerDropdown').value = expectedManufacturer;
          modelDropdown = modelDialog.$$('#modelDropdown');
          modelDropdown.value = expectedModel;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);
        })
        .then(function(args) {
          // Check that the EULA text is shown.
          assertFalse(urlElement.hidden);

          resetGetEulaUrl(cupsPrintersBrowserProxy, '' /* eulaUrl */);

          // Change ppdModel and expect |getEulaUrl| to be called again.
          modelDropdown.value = expectedModel2;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel2);
        })
        .then(function(args) {
          // Check that the EULA text is hidden.
          assertTrue(urlElement.hidden);

          resetGetEulaUrl(cupsPrintersBrowserProxy, eulaLink);

          // Change ppdModel and expect |getEulaUrl| to be called again.
          modelDropdown.value = expectedModel3;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel3);
        })
        .then(function(args) {
          assertFalse(urlElement.hidden);
        });
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled after
   * clicking it.
   */
  test('AddButtonDisabledAfterClicking', function() {
    // Starting in the discovery dialog, select the add manually button.
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$.manuallyAddPrinterButton.click();
    Polymer.dom.flush();

    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);
    clickAddButton(addDialog);
    Polymer.dom.flush();

    // Click the add button on the manufacturer dialog and then verify it is
    // disabled.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          const manufacturerDialog =
              dialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          // Populate the manufacturer and model fields to enable the add
          // button.
          manufacturerDialog.$$('#manufacturerDropdown').value = 'make';
          manufacturerDialog.$$('#modelDropdown').value = 'model';

          const addButton = manufacturerDialog.$$('#addPrinterButton');
          assertTrue(!!addButton);
          assertFalse(addButton.disabled);
          addButton.click();
          assertTrue(addButton.disabled);
        });
  });
});

suite('EditPrinterDialog', function() {
  // Sets ppdManufacturer and ppdModel since ppdManufacturer has an observer
  // that erases ppdModel when ppdManufacturer changes.
  function setPpdManufacturerAndPpdModel(manufacturer, model) {
    dialog.pendingPrinter_.ppdManufacturer = manufacturer;
    dialog.pendingPrinter_.ppdModel = model;
  }

  function clickSaveButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for save');
    const saveButton = dialog.$$('.action-button');
    dialog.printerInfoChanged_ = true;
    assertFalse(saveButton.disabled);
    assertTrue(!!saveButton, 'Button is null');
    saveButton.click();
  }

  function clickCancelButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton = dialog.$$('.cancel-button');
    assertTrue(!!cancelButton, 'Button is null');
    cancelButton.click();
  }

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  let dialog = null;

  setup(function() {
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;
    PolymerTest.clearBody();

    dialog = document.createElement('settings-cups-edit-printer-dialog');

    dialog.activePrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '',
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: '',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: '',
      printerQueue: '',
      printerStatus: '',
    };

    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '',
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: '',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: '',
      printerQueue: '',
      printerStatus: '',
    };

    dialog.isOnline_ = true;

    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
    dialog = null;
  });

  /**
   * Test that USB printers can be editted.
   */
  test('USBPrinterCanBeEdited', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: 'http://myfakeppddownload.com',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    // Set pendingPrinter_.ppdManufactuer and pendingPrinter_.ppdModel to
    // simulate a printer for which we have a PPD.
    setPpdManufacturerAndPpdModel('manufacturer', 'model');

    // Edit the printer name.
    const nameField = dialog.$$('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';
    nameField.fire('input');

    // Assert that the "Save" button is enabled.
    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that the save button is disabled when the printer address or name is
   * invalid.
   */
  test('EditPrinter', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '192.168.1.13',
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: 'HP',
      printerMakeAndModel: 'Printmaster2000',
      printerName: 'My Test Printer',
      printerPPDPath: 'http://myfakeppddownload.com',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipps',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    assertTrue(!!dialog.$$('#printerName'));
    assertTrue(!!dialog.$$('#printerAddress'));

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    // Change printer name to something valid.
    printerName = dialog.$.printerName;
    printerName.value = 'new printer name';
    printerName.fire('input');
    assertFalse(saveButton.disabled);

    // Change printer address to something invalid.
    dialog.$.printerAddress.value = 'abcdef:';
    assertTrue(saveButton.disabled);

    // Change back to something valid.
    dialog.$.printerAddress.value = 'abcdef:1234';
    assertFalse(saveButton.disabled);

    // Change printer name to empty.
    dialog.$.printerName.value = '';
    assertTrue(saveButton.disabled);
  });

  test('CloseEditDialogDoesNotModifyActivePrinter', function() {
    const expectedPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test123',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    dialog.activePrinter = Object.assign({}, expectedPrinter);

    const nameField = dialog.$$('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer name';

    const addressField = dialog.$$('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = '9.9.9.9';

    const queueField = dialog.$$('#printerQueue');
    assertTrue(!!queueField);
    queueField.value = 'edited/print';

    const protocolField = dialog.$$('.md-select');
    assertTrue(!!protocolField);
    protocolField.value = 'http';

    clickCancelButton(dialog);

    // Assert that activePrinter properties were not changed.
    assertEquals(expectedPrinter.printerName, dialog.activePrinter.printerName);
    assertEquals(
        expectedPrinter.printerAddress, dialog.activePrinter.printerAddress);
    assertEquals(
        expectedPrinter.printerQueue, dialog.activePrinter.printerQueue);
    assertEquals(
        expectedPrinter.printerProtocol, dialog.activePrinter.printerProtocol);
  });

  test('TestEditNameAndSave', function() {
    dialog.pendingPrinter_ = {
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    const expectedName = 'editedName';
    return cupsPrintersBrowserProxy
        .whenCalled('getPrinterPpdManufacturerAndModel')
        .then(function() {
          setPpdManufacturerAndPpdModel('manufacturer', 'model');

          const nameField = dialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;

          Polymer.dom.flush();
          clickSaveButton(dialog);
          return cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
        })
        .then(function() {
          assertEquals(expectedName, dialog.activePrinter.printerName);
        });
  });

  test('TestEditFieldsAndSave', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'address',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    const expectedAddress = '9.9.9.9';
    const expectedQueue = 'editedQueue';
    return cupsPrintersBrowserProxy
        .whenCalled('getPrinterPpdManufacturerAndModel')
        .then(function() {
          setPpdManufacturerAndPpdModel('manufacturer', 'model');

          // Editing more than just the printer name requires reconfiguring the
          // printer.
          const addressField = dialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = expectedAddress;

          const queueField = dialog.$$('#printerQueue');
          assertTrue(!!queueField);
          queueField.value = expectedQueue;

          clickSaveButton(dialog);
          return cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
        })
        .then(function() {
          assertEquals(expectedAddress, dialog.activePrinter.printerAddress);
          assertEquals(expectedQueue, dialog.activePrinter.printerQueue);
        });
  });

  test('TestEditAutoConfFieldsAndSave', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: true,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    const expectedAddress = '9.9.9.9';
    const expectedQueue = 'editedQueue';
    return cupsPrintersBrowserProxy
        .whenCalled('getPrinterPpdManufacturerAndModel')
        .then(function() {
          // Editing more than just the printer name requires reconfiguring the
          // printer.
          const addressField = dialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = expectedAddress;

          const queueField = dialog.$$('#printerQueue');
          assertTrue(!!queueField);
          queueField.value = expectedQueue;

          clickSaveButton(dialog);
          return cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
        })
        .then(function() {
          assertEquals(expectedAddress, dialog.activePrinter.printerAddress);
          assertEquals(expectedQueue, dialog.activePrinter.printerQueue);
        });
  });

  test('TestNonAutoConfPrintersCanSelectManufactureAndModel', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    // Assert that the manufacturer and model drop-downs are shown.
    assertFalse(dialog.$$('#makeAndModelSection').hidden);
  });

  test('TestAutoConfPrintersCannotSelectManufactureAndModel', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: true,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    // Assert that the manufacturer and model drop-downs are hidden.
    assertTrue(!dialog.$$('#makeAndModelSection').if);
  });

  test('TestChangingNameEnablesSaveButton', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };
    setPpdManufacturerAndPpdModel('manufacture', 'model');

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const nameField = dialog.$$('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';
    nameField.fire('input');

    assertTrue(!saveButton.disabled);
  });

  test('TestChangingAddressEnablesSaveButton', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };
    setPpdManufacturerAndPpdModel('manufacture', 'model');

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const addressField = dialog.$$('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = 'newAddress:789';
    addressField.fire('input');

    assertTrue(!saveButton.disabled);
  });

  test('TestChangingQueueEnablesSaveButton', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };
    setPpdManufacturerAndPpdModel('manufacture', 'model');

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const queueField = dialog.$$('#printerQueue');
    assertTrue(!!queueField);
    queueField.value = 'newqueueinfo';
    queueField.fire('input');

    assertTrue(!saveButton.disabled);
  });

  test('TestChangingProtocolEnablesSaveButton', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };
    setPpdManufacturerAndPpdModel('manufacture', 'model');
    Polymer.dom.flush();

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const dropDown = dialog.$$('.md-select');
    dropDown.value = 'http';
    dropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    Polymer.dom.flush();
    assertTrue(!saveButton.disabled);
  });

  test('TestChangingModelEnablesSaveButton', function() {
    dialog.pendingPrinter_ = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };
    setPpdManufacturerAndPpdModel('manufacture', 'model');
    Polymer.dom.flush();

    // Printers are considered initialized for editing after PPD make/models
    // are set.
    dialog.arePrinterFieldsInitialized_ = true;

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    makeDropDown = dialog.$$('#printerPPDManufacturer');
    makeDropDown.value = 'HP';
    makeDropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    // Saving is disabled until a model is selected.
    assertTrue(saveButton.disabled);

    modelDropDown = dialog.$$('#printerPPDModel');
    modelDropDown.value = 'HP 910';
    modelDropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    Polymer.dom.flush();
    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that we are checking if a printer model has an EULA upon a model
   * change.
   */
  test('getEulaUrlGetsCalledOnModelChange', function() {
    const eulaLink = 'google.com';
    const expectedManufacturer = 'Google';
    const expectedModel = 'model';
    const expectedModel2 = 'newModel';
    const expectedModel3 = 'newModel2';

    let modelDropdown = null;
    let urlElement = null;

    return test_util.flushTasks()
        .then(function() {
          urlElement = dialog.$$('#eulaUrl');
          // Check that the EULA text is hidden.
          assertTrue(urlElement.hidden);

          cupsPrintersBrowserProxy.setEulaUrl(eulaLink);

          dialog.$$('#printerPPDManufacturer').value = expectedManufacturer;
          modelDropdown = dialog.$$('#printerPPDModel');
          modelDropdown.value = expectedModel;

          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);
        })
        .then(function() {
          // Check that the EULA text is shown.
          assertFalse(urlElement.hidden);

          resetGetEulaUrl(cupsPrintersBrowserProxy, '' /* eulaUrl */);

          // Change ppdModel and expect |getEulaUrl| to be called again.
          modelDropdown.value = expectedModel2;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel2);
        })
        .then(function() {
          // Check that the EULA text is hidden.
          assertTrue(urlElement.hidden);

          resetGetEulaUrl(cupsPrintersBrowserProxy, eulaLink);

          // Change ppdModel and expect |getEulaUrl| to be called again.
          modelDropdown.value = expectedModel3;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel3);
        })
        .then(function() {
          // Check that the EULA text is shown again.
          assertFalse(urlElement.hidden);
        });
  });

  test('OfflineEdit', function() {
    dialog.pendingPrinter_ = {
      printerDescription: '',
      printerId: 'id_123',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    const expectedName = 'editedName';
    return cupsPrintersBrowserProxy
        .whenCalled('getPrinterPpdManufacturerAndModel')
        .then(function() {
          setPpdManufacturerAndPpdModel('manufacture', 'model');

          // Simulate offline.
          // TODO(jimmyxgong): Use NetworkConfigFake instead of directly
          // changing this private variable.
          dialog.isOnline_ = false;

          const nameField = dialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          Polymer.dom.flush();

          const saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertFalse(saveButton.disabled);

          clickSaveButton(dialog);
          return cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
        })
        .then(function() {
          assertEquals(expectedName, dialog.activePrinter.printerName);
        });
  });
});
