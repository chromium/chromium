// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {CupsPrintersBrowserProxyImpl,PrinterSetupResult,CupsPrintersEntryManager,PrintServerResult,PrinterType} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.m.js';
// #import {createCupsPrinterInfo,createPrinterListEntry} from './cups_printer_test_utils.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flushTasks} from '../../test_util.m.js';
// #import {MojoInterfaceProviderImpl, MojoInterfaceProvider} from '//resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// clang-format on

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

  function mockAddPrinterInputKeyboardPress(crInputId) {
    // Start in add manual dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);

    // Test that pressing Enter before all the fields are populated does not
    // advance to the next dialog.
    const input = addDialog.$$(crInputId);
    MockInteractions.keyEventOn(input, 'keypress', /*keycode=*/13, [], 'Enter');
    Polymer.dom.flush();

    assertFalse(!!dialog.$$('add-printer-manufacturer-model-dialog'));
    assertFalse(dialog.showManufacturerDialog_);
    assertTrue(dialog.showManuallyAddDialog_);

    // Add valid input into the dialog
    fillAddManuallyDialog(addDialog);

    // Test that key press on random key while in input field is not accepted as
    // as valid Enter press.
    MockInteractions.keyEventOn(input, 'keypress', /*keycode=*/16, [], 'Shift');
    Polymer.dom.flush();

    assertFalse(!!dialog.$$('add-printer-manufacturer-model-dialog'));
    assertFalse(dialog.showManufacturerDialog_);
    assertTrue(dialog.showManuallyAddDialog_);

    // Now test Enter press with valid input.
    MockInteractions.keyEventOn(input, 'keypress', /*keycode=*/13, [], 'Enter');
    Polymer.dom.flush();
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
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = false;
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
    // Starts in add manual dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();
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
          assertTrue(!!dialog.$$('add-printer-manufacturer-model-dialog'));

          assertTrue(dialog.showManufacturerDialog_);
          assertFalse(dialog.showManuallyAddDialog_);
        });
  });

  /**
   * Test that when getPrinterInfo fails for a generic reason, the general error
   * message is shown.
   */
  test('GetPrinterInfoFailsGeneralError', function() {
    // Starts in add manual dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();

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
   * Test that when getPrinterInfo fails for an unreachable printer, the
   printer
   * address field is marked as invalid.
   */
  test('GetPrinterInfoFailsUnreachableError', function() {
    // Starts in add manual dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();

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
    // Starts in add manual dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();
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
    // Start on add manual dialog.
    dialog.fire('open-manually-add-printer-dialog');
    Polymer.dom.flush();

    // Populate the printer object.
    dialog.newPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '192.168.1.13',
      printerDescription: '',
      printerId: '',
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
    Polymer.dom.flush();
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
   * Test that we are checking if a printer model has an EULA upon a model
   * change.
   */
  test('getEulaUrlGetsCalledOnModelChange', function() {
    // Start in add manual dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();
    fillAddManuallyDialog(addDialog);

    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    const expectedEulaLink = 'chrome://os-credits/#google';
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

          cupsPrintersBrowserProxy.setEulaUrl(expectedEulaLink);

          modelDialog.$$('#manufacturerDropdown').value = expectedManufacturer;
          modelDropdown = modelDialog.$$('#modelDropdown');
          modelDropdown.value = expectedModel;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);
        })
        .then(function(args) {
          // Check that the EULA text is shown.
          assertFalse(urlElement.hidden);
          assertEquals(expectedEulaLink, urlElement.querySelector('a').href);

          resetGetEulaUrl(cupsPrintersBrowserProxy, '' /* eulaUrl */);

          // Change ppdModel and expect |getEulaUrl| to be called again.
          modelDropdown.value = expectedModel2;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel2);
        })
        .then(function(args) {
          // Check that the EULA text is hidden.
          assertTrue(urlElement.hidden);

          resetGetEulaUrl(cupsPrintersBrowserProxy, expectedEulaLink);

          // Change ppdModel and expect |getEulaUrl| to be called again.
          modelDropdown.value = expectedModel3;
          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel3);
        })
        .then(function(args) {
          assertFalse(urlElement.hidden);
          assertEquals(expectedEulaLink, urlElement.querySelector('a').href);
        });
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled after
   * clicking it.
   */
  test('AddButtonDisabledAfterClicking', function() {
    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();
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

  /**
   * The following tests check that clicking Enter button on the keyboard
   from
   * each input text field on the add-printer-manually-dialog will advance to
   * the next dialog.
   */
  test('PressEnterInPrinterNameInput', function() {
    mockAddPrinterInputKeyboardPress('#printerNameInput');

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          return test_util.flushTasks();
        })
        .then(function() {
          // Showing model selection.
          assertTrue(!!dialog.$$('add-printer-manufacturer-model-dialog'));
          assertTrue(dialog.showManufacturerDialog_);
          assertFalse(dialog.showManuallyAddDialog_);
        });
  });

  test('PressEnterInPrinterAddressInput', function() {
    mockAddPrinterInputKeyboardPress('#printerAddressInput');

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          return test_util.flushTasks();
        })
        .then(function() {
          // Showing model selection.
          assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
          assertTrue(dialog.showManufacturerDialog_);
          assertFalse(dialog.showManuallyAddDialog_);
        });
  });

  test('PressEnterInPrinterQueueInput', function() {
    mockAddPrinterInputKeyboardPress('#printerQueueInput');

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          return test_util.flushTasks();
        })
        .then(function() {
          // Showing model selection.
          assertTrue(!!dialog.$$('add-printer-manufacturer-model-dialog'));
          assertTrue(dialog.showManufacturerDialog_);
          assertFalse(dialog.showManuallyAddDialog_);
        });
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled when the
   * manufacturer or model dropdown has an incorrect value.
   */
  test('AddButtonDisabledAfterClicking', function() {
    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    Polymer.dom.flush();
    fillAddManuallyDialog(addDialog);
    clickAddButton(addDialog);
    Polymer.dom.flush();

    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          const manufacturerDialog =
              dialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          const manufacturerDropdown =
              manufacturerDialog.$$('#manufacturerDropdown');
          const modelDropdown =
              manufacturerDialog.$$('#modelDropdown');
          const addButton = manufacturerDialog.$$('#addPrinterButton');

          // Set the starting values for manufacturer and model dropdown.
          manufacturerDropdown.value = 'make';
          modelDropdown.value = 'model';
          assertFalse(addButton.disabled);

          // Mimic typing in random input. Make sure the Add button becomes
          // disabled.
          manufacturerDropdown.$$('#search').value = 'hlrRkJQkNsh';
          manufacturerDropdown.$$('#search').fire('input');
          assertTrue(addButton.disabled);

          // Then mimic typing in the original value to re-enable the Add
          // button.
          manufacturerDropdown.$$('#search').value = 'make';
          manufacturerDropdown.$$('#search').fire('input');
          assertFalse(addButton.disabled);

          // Mimic typing in random input. Make sure the Add button becomes
          // disabled.
          modelDropdown.$$('#search').value = 'hlrRkJQkNsh';
          modelDropdown.$$('#search').fire('input');
          assertTrue(addButton.disabled);

          // Then mimic typing in the original value to re-enable the Add
          // button.
          modelDropdown.$$('#search').value = 'model';
          modelDropdown.$$('#search').fire('input');
          assertFalse(addButton.disabled);
        });
  });

  test('Queue input is hidden when protocol is App Socket', () => {
    const addDialog = dialog.$$('add-printer-manually-dialog');
    let printerQueueInput = addDialog.$$('#printerQueueInput');
    const select = addDialog.shadowRoot.querySelector('select');
    assertTrue(!!printerQueueInput);

    select.value = 'socket';
    select.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    Polymer.dom.flush();

    printerQueueInput = addDialog.$$('#printerQueueInput');
    assertFalse(!!printerQueueInput);

    select.value = 'http';
    select.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    Polymer.dom.flush();

    printerQueueInput = addDialog.$$('#printerQueueInput');
    assertTrue(!!printerQueueInput);
  });
});

suite('EditPrinterDialog', function() {
  /** @type {!HTMLElement} */
  let page = null;
  /** @type {!HTMLElement} */
  let dialog = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type {!chromeos.networkConfig.mojom.NetworkStateProperties|undefined} */
  let wifi1;

  setup(function() {
    const mojom = chromeos.networkConfig.mojom;

    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    // Simulate internet connection.
    wifi1 = OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = mojom.ConnectionStateType.kOnline;

    PolymerTest.clearBody();
    settings.Router.getInstance().navigateTo(settings.routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    page.onActiveNetworksChanged([wifi1]);
    Polymer.dom.flush();
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog = null;
    page = null;
  });

  /**
   * @param {!HTMLElement} dialog
   * @private
   */
  function clickSaveButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for save');
    const saveButton = dialog.$$('.action-button');
    dialog.printerInfoChanged_ = true;
    assertFalse(saveButton.disabled);
    assertTrue(!!saveButton, 'Button is null');
    saveButton.click();
  }

  /**
   * @param {!HTMLElement} dialog
   * @private
   */
  function clickCancelButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton = dialog.$$('.cancel-button');
    assertTrue(!!cancelButton, 'Button is null');
    cancelButton.click();
  }

  /**
   * Initializes a printer and sets that printer as the printer to be edited in
   * the edit dialog. Opens the edit dialog.
   * @param {!CupsPrinterInfo} printerInfo
   * @param {boolean} autoconf
   * @param {string} manufacturer
   * @param {string} model
   * @param {string} protocol
   * @param {string} serverAddress
   * @param {Promise} Returns a promise after the edit dialog is initialized.
   * @private
   */
  function initializeAndOpenEditDialog(
      name, address, id, autoconf, manufacturer, model, protocol,
      serverAddress) {
    page.activePrinter =
        cups_printer_test_util.createCupsPrinterInfo(name, address, id);
    page.activePrinter.printerPpdReference.autoconf = autoconf;
    page.activePrinter.printerProtocol = protocol;
    page.activePrinter.printServerUri = serverAddress;
    cupsPrintersBrowserProxy.printerPpdMakeModel = {
      ppdManufacturer: manufacturer,
      ppdModel: model
    };
    // Trigger the edit dialog to open.
    page.fire('edit-cups-printer-details');
    Polymer.dom.flush();
    dialog = page.$$('settings-cups-edit-printer-dialog');
    // This proxy function gets called whenever the edit dialog is initialized.
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList');
  }

  /**
   * Test that USB printers can be edited.
   */
  test('USBPrinterCanBeEdited', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'usb', /*serverAddress=*/ '')
        .then(() => {
          // Assert that the protocol is USB.
          assertEquals('usb', dialog.$$('.md-select').value);

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
  });

  /**
   * Test that the save button is disabled when the printer address or name is
   * invalid.
   */
  test('EditPrinter', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          assertTrue(!!dialog.$$('#printerName'));
          assertTrue(!!dialog.$$('#printerAddress'));

          const saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertTrue(saveButton.disabled);

          // Change printer name to something valid.
          const printerName = dialog.$.printerName;
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
  });

  /**
   * Test that closing the dialog does not persist the edits.
   */
  test('CloseEditDialogDoesNotModifyActivePrinter', function() {
    const expectedName = 'Test Printer';
    const expectedAddress = '1.1.1.1';
    const expectedId = 'ID1';
    const expectedProtocol = 'ipp';
    return initializeAndOpenEditDialog(
               /*name=*/ expectedName, /*address=*/ expectedAddress,
               /*id=*/ expectedId, /*autoconf=*/ false,
               /*manufacturer=*/ 'make', /*model=*/ 'model',
               /*protocol=*/ expectedProtocol, /*serverAddress=*/ '')
        .then(() => {
          const nameField = dialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = 'edited printer name';

          const addressField = dialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = '9.9.9.9';

          const protocolField = dialog.$$('.md-select');
          assertTrue(!!protocolField);
          protocolField.value = 'http';

          clickCancelButton(dialog);

          // Assert that activePrinter properties were not changed.
          assertEquals(expectedName, dialog.activePrinter.printerName);
          assertEquals(expectedAddress, dialog.activePrinter.printerAddress);
          assertEquals(expectedProtocol, dialog.activePrinter.printerProtocol);
        });
  });

  /**
   * Test that editing the name field results in properly saving the new name.
   */
  test('TestEditNameAndSave', function() {
    const expectedName = 'editedName';
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
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

  /**
   * Test that editing various fields results in properly saving the new
   * changes.
   */
  test('TestEditFieldsAndSave', function() {
    const expectedAddress = '9.9.9.9';
    const expectedQueue = 'editedQueue';
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
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

  /**
   * Test that editing an autoconf printer saves correctly.
   */
  test('TestEditAutoConfFieldsAndSave', function() {
    const expectedAddress = '9.9.9.9';
    const expectedQueue = 'editedQueue';
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ true, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
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

  /**
   * Test that non-autoconf printers can select the make and model dropdowns.
   */
  test('TestNonAutoConfPrintersCanSelectManufactureAndModel', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          // Assert that the manufacturer and model drop-downs are shown.
          assertFalse(dialog.$$('#makeAndModelSection').hidden);
        });
  });

  /**
   * Test that autoconf printers cannot select their make/model
   */
  test('TestAutoConfPrintersCannotSelectManufactureAndModel', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ true, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          // Assert that the manufacturer and model drop-downs are hidden.
          assertTrue(!dialog.$$('#makeAndModelSection').if);
        });
  });

  /**
   * Test that changing the name enables the save button.
   */
  test('TestChangingNameEnablesSaveButton', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          const saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertTrue(saveButton.disabled);

          const nameField = dialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = 'edited printer';
          nameField.fire('input');

          assertTrue(!saveButton.disabled);
        });
  });

  /**
   * Test that changing the address enables the save button.
   */
  test('TestChangingAddressEnablesSaveButton', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          const saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertTrue(saveButton.disabled);

          const addressField = dialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = 'newAddress:789';
          addressField.fire('input');

          assertTrue(!saveButton.disabled);
        });
  });

  /**
   * Test that changing the queue enables the save button.
   */
  test('TestChangingQueueEnablesSaveButton', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          const saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertTrue(saveButton.disabled);

          const queueField = dialog.$$('#printerQueue');
          assertTrue(!!queueField);
          queueField.value = 'newqueueinfo';
          queueField.fire('input');

          assertTrue(!saveButton.disabled);
        });
  });

  /**
   * Test that changing the protocol enables the save button.
   */
  test('TestChangingProtocolEnablesSaveButton', function() {
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          const saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertTrue(saveButton.disabled);

          const dropDown = dialog.$$('.md-select');
          dropDown.value = 'http';
          dropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
          Polymer.dom.flush();
          assertTrue(!saveButton.disabled);
        });
  });

  /**
   * Test that changing the model enables the save button.
   */
  test('TestChangingModelEnablesSaveButton', function() {
    let saveButton = null;

    cupsPrintersBrowserProxy.manufacturers = {
      success: true,
      manufacturers: ['HP']
    };
    cupsPrintersBrowserProxy.models = {success: true, models: ['HP 910']};
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          saveButton = dialog.$$('.action-button');
          assertTrue(!!saveButton);
          assertTrue(saveButton.disabled);

          const makeDropDown = dialog.$$('#printerPPDManufacturer');
          makeDropDown.value = 'HP';
          makeDropDown.dispatchEvent(
              new CustomEvent('change'), {'bubbles': true});

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterModelsList');
        })
        .then(() => {
          // Saving is disabled until a model is selected.
          assertTrue(saveButton.disabled);

          const modelDropDown = dialog.$$('#printerPPDModel');
          modelDropDown.value = 'HP 910';
          modelDropDown.dispatchEvent(
              new CustomEvent('change'), {'bubbles': true});

          Polymer.dom.flush();
          assertTrue(!saveButton.disabled);
        });
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
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
          urlElement = dialog.$$('#eulaUrl');
          // Check that the EULA text is hidden.
          assertTrue(urlElement.hidden);

          // 'getEulaUrl' is called as part of the initialization of the dialog,
          // so we have to reset the resolver before the next call.
          resetGetEulaUrl(cupsPrintersBrowserProxy, eulaLink);

          dialog.$$('#printerPPDManufacturer').value = expectedManufacturer;
          modelDropdown = dialog.$$('#printerPPDModel');
          modelDropdown.value = expectedModel;

          return verifyGetEulaUrlWasCalled(
              cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);
        })
        .then(function() {
          // Check that the EULA text is shown.
          assertFalse(urlElement.hidden);

          resetGetEulaUrl(cupsPrintersBrowserProxy, /*eulaUrl=*/ '');

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

  /**
   * Test that editing the name is still supported when offline.
   */
  test('OfflineEdit', function() {
    // Simulate connecting to a network with no internet connection.
    wifi1.connectionState =
        chromeos.networkConfig.mojom.ConnectionStateType.kConnected;
    page.onActiveNetworksChanged([wifi1]);
    Polymer.dom.flush();
    const expectedName = 'editedName';
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ false, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '')
        .then(() => {
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

  test('PrintServerPrinterEdit', function() {
    const expectedName = 'edited name';
    return initializeAndOpenEditDialog(
               /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
               /*autoconf=*/ true, /*manufacturer=*/ 'make',
               /*model=*/ 'model', /*protocol=*/ 'ipp',
               /*serverAddress=*/ 'ipp://192.168.1.1:631')
        .then(() => {
          // Verify the only the name field is not disabled.
          assertTrue(dialog.$$('#printerAddress').disabled);
          assertTrue(dialog.$$('.md-select').disabled);
          assertTrue(dialog.$$('#printerQueue').disabled);

          const nameField = dialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          assertFalse(nameField.disabled);

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

suite('PrintServerTests', function() {
  let page = null;
  let dialog = null;

  /** @type {?settings.printing.CupsPrintersEntryManager} */
  let entryManager = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;


  setup(function() {
    entryManager = settings.printing.CupsPrintersEntryManager.getInstance();
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*automaticPrinters=*/[],
        /*discoveredPrinters=*/[], /*printServerPrinters=*/[]);

    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    PolymerTest.clearBody();
    settings.Router.getInstance().navigateTo(settings.routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    dialog = page.$$('settings-cups-add-printer-dialog');
    assertTrue(!!dialog);

    Polymer.dom.flush();
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog = null;
    page = null;
  });

  /**
   * @param {!Array<!PrinterListEntry>} savedPrinters
   * @param {!Array<!CupsPrinterInfo>} automaticPrinters
   * @param {!Array<!CupsPrinterInfo>} discoveredPrinters
   * @param {!Array<!PrinterListEntry>} printerServerPrinters
   */
  function setEntryManagerPrinters(
      savedPrinters, automaticPrinters, discoveredPrinters,
      printerServerPrinters) {
    entryManager.setSavedPrintersList(savedPrinters);
    entryManager.setNearbyPrintersList(automaticPrinters, discoveredPrinters);
    entryManager.printServerPrinters = printerServerPrinters;
  }

  /**
   * @param {!HTMLElement} page
   * @return {?HTMLElement} Returns the print server dialog if it is available.
   * @private
   */
  function getPrintServerDialog(page) {
    assertTrue(!!page);
    dialog = page.$$('settings-cups-add-printer-dialog');
    return dialog.$$('add-print-server-dialog');
  }

  /**
   * Opens the add print server dialog, inputs |address| with the specified
   * |error|. Adds the print server and returns a promise for handling the add
   * event.
   * @param {string} address
   * @param {number} error
   * @return {!Promise} The promise returned when queryPrintServer is called.
   * @private
   */
  function addPrintServer(address, error) {
    // Open the add manual printe dialog.
    assertTrue(!!page);
    dialog.open();
    Polymer.dom.flush();

    const addPrinterDialog = dialog.$$('add-printer-manually-dialog');
    // Switch to Add print server dialog.
    addPrinterDialog.$$('#print-server-button').click();
    Polymer.dom.flush();
    const printServerDialog = dialog.$$('add-print-server-dialog');
    assertTrue(!!printServerDialog);

    Polymer.dom.flush();
    cupsPrintersBrowserProxy.setQueryPrintServerResult(error);
    return test_util.flushTasks().then(() => {
      // Fill dialog with the server address.
      const address = printServerDialog.$$('#printServerAddressInput');
      assertTrue(!!address);
      address.value = address;

      // Add the print server.
      const button = printServerDialog.$$('.action-button');
      // Button should not be disabled before clicking on it.
      assertTrue(!button.disabled);
      button.click();

      // Clicking on the button should disable it.
      assertTrue(button.disabled);
      return cupsPrintersBrowserProxy.whenCalled('queryPrintServer');
    });
  }

  /**
   * @param {string} expectedError
   * @private
   */
  function verifyErrorMessage(expectedError) {
    // Assert that the dialog did not close on errors.
    const printServerDialog = getPrintServerDialog(page);
    const dialogError = printServerDialog.$$('#server-dialog-error');
    // Assert that the dialog error is displayed.
    assertTrue(!dialogError.hidden);
    assertEquals(loadTimeData.getString(expectedError), dialogError.errorText);
  }

  /**
   * @param {string} expectedMessage
   * @param {number} numPrinters
   * @private
   */
  function verifyToastMessage(expectedMessage, numPrinters) {
    // We always display the total number of printers found from a print
    // server.
    const toast = page.$$('#printServerErrorToast');
    assertTrue(toast.open);
    assertEquals(
        loadTimeData.getStringF(expectedMessage, numPrinters),
        toast.textContent.trim());
  }

  test('AddPrintServerIsSuccessful', function() {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{!CupsPrintServerPrintersInfo} */ ({
          printerList: [
            cups_printer_test_util.createCupsPrinterInfo(
                'nameA', 'serverAddress', 'idA'),
            cups_printer_test_util.createCupsPrinterInfo(
                'nameB', 'serverAddress', 'idB')
          ]
        });
    return addPrintServer('serverAddress', PrintServerResult.NO_ERRORS)
        .then(() => {
          Polymer.dom.flush();
          verifyToastMessage(
              'printServerFoundManyPrinters', /*numPrinters=*/ 2);
          assertEquals(2, entryManager.printServerPrinters.length);
        });
  });

  test('HandleDuplicateQueries', function() {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{!CupsPrintServerPrintersInfo} */ ({
          printerList: [
            cups_printer_test_util.createCupsPrinterInfo(
                'nameA', 'serverAddress', 'idA'),
            cups_printer_test_util.createCupsPrinterInfo(
                'nameB', 'serverAddress', 'idB')
          ]
        });

    return test_util.flushTasks()
        .then(() => {
          // Simulate that a print server was queried previously.
          setEntryManagerPrinters(
              /*savedPrinters=*/[], /*nearbyPrinters=*/[],
              /*discoveredPrinters=*/[], [
                cups_printer_test_util.createPrinterListEntry(
                    'nameA', 'serverAddress', 'idA', PrinterType.PRINTSERVER),
                cups_printer_test_util.createPrinterListEntry(
                    'nameB', 'serverAddress', 'idB', PrinterType.PRINTSERVER)
              ]);
          Polymer.dom.flush();
          assertEquals(2, entryManager.printServerPrinters.length);

          // This will attempt to add duplicate print server printers.
          // Matching printerId's are considered duplicates.
          return addPrintServer('serverAddress', PrintServerResult.NO_ERRORS);
        })
        .then(() => {
          Polymer.dom.flush();

          verifyToastMessage(
              'printServerFoundManyPrinters', /*numPrinters=*/ 2);
          // Assert that adding the same print server results in no new printers
          // added to the entry manager.
          assertEquals(2, entryManager.printServerPrinters.length);
          const nearbyPrintersElement =
              page.$$('settings-cups-nearby-printers');
          assertEquals(2, nearbyPrintersElement.nearbyPrinters.length);
        });
  });

  test('HandleDuplicateSavedPrinters', function() {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({
          printerList: [
            cups_printer_test_util.createCupsPrinterInfo(
                'nameA', 'serverAddress', 'idA'),
            cups_printer_test_util.createCupsPrinterInfo(
                'nameB', 'serverAddress', 'idB')
          ]
        });

    return test_util.flushTasks().then(() => {
      // Simulate that a print server was queried previously.
      setEntryManagerPrinters(
          /*savedPrinters=*/[], /*nearbyPrinters=*/[],
          /*discoveredPrinters=*/[], [
            cups_printer_test_util.createPrinterListEntry(
                'nameA', 'serverAddress', 'idA', PrinterType.PRINTSERVER),
            cups_printer_test_util.createPrinterListEntry(
                'nameB', 'serverAddress', 'idB', PrinterType.PRINTSERVER)
          ]);
      Polymer.dom.flush();
      assertEquals(2, entryManager.printServerPrinters.length);

      // Simulate adding a saved printer.
      entryManager.setSavedPrintersList(
          [cups_printer_test_util.createPrinterListEntry(
              'nameA', 'serverAddress', 'idA', PrinterType.SAVED)]);
      Polymer.dom.flush();

      // Simulate the underlying model changes. Nearby printers are also
      // updated after changes to saved printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', /*automaticPrinter=*/[],
          /*discoveredPrinters=*/[]);
      Polymer.dom.flush();

      // Verify that we now only have 1 printer in print server printers
      // list.
      assertEquals(1, entryManager.printServerPrinters.length);
      const nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertEquals(1, nearbyPrintersElement.nearbyPrinters.length);
      // Verify we correctly removed the duplicate printer, 'idA', since
      // it exists in the saved printer list. Expect only 'idB' in
      // the print server printers list.
      assertEquals(
          'idB', entryManager.printServerPrinters[0].printerInfo.printerId);
    });
  });

  test('AddPrintServerAddressError', function() {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    return addPrintServer('serverAddress', PrintServerResult.INCORRECT_URL)
        .then(() => {
          Polymer.dom.flush();
          const printServerDialog = getPrintServerDialog(page);
          // Assert that the dialog did not close on errors.
          assertTrue(!!printServerDialog);
          // Assert that the address input field is invalid.
          assertTrue(printServerDialog.$$('#printServerAddressInput').invalid);
        });
  });

  test('AddPrintServerConnectionError', function() {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    return addPrintServer('serverAddress', PrintServerResult.CONNECTION_ERROR)
        .then(() => {
          Polymer.dom.flush();
          verifyErrorMessage('printServerConnectionError');
        });
  });

  test('AddPrintServerReachableServerButIppResponseError', function() {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    return addPrintServer(
               'serverAddress', PrintServerResult.CANNOT_PARSE_IPP_RESPONSE)
        .then(() => {
          Polymer.dom.flush();
          verifyErrorMessage('printServerConfigurationErrorMessage');
        });
  });

  test('AddPrintServerReachableServerButHttpResponseError', function() {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    return addPrintServer('serverAddress', PrintServerResult.HTTP_ERROR)
        .then(() => {
          Polymer.dom.flush();
          verifyErrorMessage('printServerConfigurationErrorMessage');
        });
  });
});
