// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CupsPrintersBrowserProxyImpl, CupsPrintersEntryManager, PrinterSettingsUserAction, PrinterSetupResult, PrinterType, PrintServerResult} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createCupsPrinterInfo, createPrinterListEntry} from './cups_printer_test_utils.js';
import {FakeMetricsPrivate} from './fake_metrics_private.js';
import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.js';

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

suite('CupsPrinterUITests', () => {
  let page = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  setup(async () => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    await resetPage();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    page = null;
  });

  function resetPage() {
    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    return flushTasks();
  }

  // Verify the Saved printers section strings.
  test('SavedPrintersText', async () => {
    page.savedPrinters_ = [createPrinterListEntry(
        'nameA', 'printerAddress', 'idA', PrinterType.SAVED)];

    await flushTasks();
    assertEquals(
        loadTimeData.getString('savedPrintersSubtext'),
        page.shadowRoot.querySelector('#savedPrintersContainer .secondary')
            .textContent.trim());
  });

  // Verify the Nearby printers section strings.
  test('AvailablePrintersText', async () => {
    assertEquals(
        loadTimeData.getString('availablePrintersReadyTitle'),
        page.shadowRoot.querySelector('#availablePrintersReadyTitle')
            .textContent.trim());
    assertEquals(
        loadTimeData.getString('availablePrintersReadySubtext'),
        page.shadowRoot.querySelector('#availablePrintersReadySubtext')
            .textContent.trim());
  });

  // Verify the Nearby printers section can be properly opened and closed.
  test('CollapsibleNearbyPrinterSection', async () => {
    page.canAddPrinter = true;

    // The Add printer section above the Nearby printers section should be
    // hidden.
    assertFalse(isVisible(page.shadowRoot.querySelector('#addPrinterSection')));

    // The collapsible section should start opened, then after clicking the
    // button should close.
    const toggleButton =
        page.shadowRoot.querySelector('#nearbyPrinterToggleButton');
    assertTrue(isVisible(page.shadowRoot.querySelector('#collapsibleSection')));
    assertTrue(isVisible(page.shadowRoot.querySelector('#helpSection')));
    toggleButton.click();
    assertFalse(
        isVisible(page.shadowRoot.querySelector('#collapsibleSection')));
    assertFalse(isVisible(page.shadowRoot.querySelector('#helpSection')));
    toggleButton.click();
    assertTrue(isVisible(page.shadowRoot.querySelector('#collapsibleSection')));
    assertTrue(isVisible(page.shadowRoot.querySelector('#helpSection')));
  });

  // Verify the Saved printers empty state only shows when there are no saved
  // printers.
  test('SavedPrintersEmptyState', async () => {
    // Settings should start in empty state without saved printers.
    const emptyState = page.shadowRoot.querySelector('#noSavedPrinters');
    assertTrue(isVisible(emptyState));

    await flushTasks();

    // Add a saved printer and expect the empty state to be hidden.
    webUIListenerCallback('on-saved-printers-changed', {
      printerList: [
        createCupsPrinterInfo('nameA', 'address', 'idA'),
      ],
    });
    assertFalse(isVisible(emptyState));
  });

  // Verify the Nearby printers section starts open when there are no saved
  // printers or open when there's more than one saved printer.
  test('CollapsibleNearbyPrinterSectionSavedPrinters', async () => {
    // Simulate no saved printers and expect the section to be open.
    cupsPrintersBrowserProxy.printerList = {printerList: []};
    resetPage();
    await flushTasks();
    assertTrue(isVisible(page.shadowRoot.querySelector('#collapsibleSection')));

    // Simulate 1 saved printer on load and expect the section to be
    // collapsed.
    cupsPrintersBrowserProxy.printerList = {
      printerList: [
        createCupsPrinterInfo('nameA', 'address', 'idA'),
      ],
    };
    resetPage();
    await flushTasks();
    assertFalse(
        isVisible(page.shadowRoot.querySelector('#collapsibleSection')));
  });

  // Verify clicking the add printer manually button is recorded to metrics.
  test('RecordUserActionMetric', async () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    // Enable the add printer manually button.
    page.prefs = {
      native_printing: {
        user_native_printers_allowed: {
          value: true,
        },
      },
    };
    page.canAddPrinter = true;

    await flushTasks();
    page.shadowRoot.querySelector('.add-manual-printer-icon').click();
    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'Printing.CUPS.SettingsUserAction',
            PrinterSettingsUserAction.ADD_PRINTER_MANUALLY));
  });
});

suite('CupsAddPrinterDialogTests', function() {
  function fillAddManuallyDialog(addDialog) {
    const name = addDialog.shadowRoot.querySelector('#printerNameInput');
    const address = addDialog.shadowRoot.querySelector('#printerAddressInput');

    assertTrue(!!name);
    name.value = 'Test printer';

    assertTrue(!!address);
    address.value = '127.0.0.1';

    const addButton = addDialog.shadowRoot.querySelector('#addPrinterButton');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
  }

  function clickAddButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for add');
    const addButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!addButton, 'Button is null');
    addButton.click();
  }

  function clickCancelButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton = dialog.shadowRoot.querySelector('.cancel-button');
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
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);

    // Test that pressing Enter before all the fields are populated does not
    // advance to the next dialog.
    const input = addDialog.shadowRoot.querySelector(crInputId);
    keyEventOn(input, 'keypress', /*keycode=*/ 13, [], 'Enter');
    flush();

    assertFalse(!!dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertFalse(dialog.showManufacturerDialog_);
    assertTrue(dialog.showManuallyAddDialog_);

    // Add valid input into the dialog
    fillAddManuallyDialog(addDialog);

    // Test that key press on random key while in input field is not accepted as
    // as valid Enter press.
    keyEventOn(input, 'keypress', /*keycode=*/ 16, [], 'Shift');
    flush();

    assertFalse(!!dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertFalse(dialog.showManufacturerDialog_);
    assertTrue(dialog.showManuallyAddDialog_);

    // Now test Enter press with valid input.
    keyEventOn(input, 'keypress', /*keycode=*/ 13, [], 'Enter');
    flush();
  }

  let page = null;
  let dialog = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  setup(() => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    PolymerTest.clearBody();
    page = document.createElement('settings-cups-printers');
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = false;
    document.body.appendChild(page);
    assertTrue(!!page);
    dialog = page.shadowRoot.querySelector('settings-cups-add-printer-dialog');
    assertTrue(!!dialog);

    dialog.open();
    return flushTasks();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog = null;
    page = null;
  });

  test('ValidIPV4', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');
    assertTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1'));
  });

  test('ValidIPV4WithPort', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1:1234'));
  });

  test('ValidIPV6', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    // Test the full ipv6 address scheme.
    assertTrue(canAddPrinter(dialog, 'Test printer', '1:2:a3:ff4:5:6:7:8'));

    // Test the shorthand prefix scheme.
    assertTrue(canAddPrinter(dialog, 'Test printer', '::255'));

    // Test the shorthand suffix scheme.
    assertTrue(canAddPrinter(dialog, 'Test printer', '1::'));
  });

  test('ValidIPV6WithPort', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertTrue(canAddPrinter(dialog, 'Test printer', '[1:2:aa2:4]:12'));
    assertTrue(canAddPrinter(dialog, 'Test printer', '[::255]:54'));
    assertTrue(canAddPrinter(dialog, 'Test printer', '[1::]:7899'));
  });

  test('InvalidIPV6', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:4:5:6:7:8:9'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:aa:a1245:2'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:za:2'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1:::22'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1::2::3'));
  });

  test('ValidHostname', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertTrue(canAddPrinter(dialog, 'Test printer', 'hello-world.com'));
    assertTrue(canAddPrinter(dialog, 'Test printer', 'hello.world.com:12345'));
  });

  test('InvalidHostname', async () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertFalse(canAddPrinter(dialog, 'Test printer', 'helloworld!123.com'));
    assertFalse(canAddPrinter(dialog, 'Test printer', 'helloworld123-.com'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '-helloworld123.com'));
  });

  /**
   * Test that clicking on Add opens the model select page.
   */
  test('ValidAddOpensModelSelection', async () => {
    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);

    addDialog.shadowRoot.querySelector('.action-button').click();
    flush();

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertTrue(!!dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog'));

    assertTrue(dialog.showManufacturerDialog_);
    assertFalse(dialog.showManuallyAddDialog_);
  });

  /**
   * Test that when getPrinterInfo fails for a generic reason, the general error
   * message is shown.
   */
  test('GetPrinterInfoFailsGeneralError', async () => {
    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();

    fillAddManuallyDialog(addDialog);

    // Make the getPrinterInfo fail for a generic error.
    cupsPrintersBrowserProxy.setGetPrinterInfoResult(
        PrinterSetupResult.FATAL_ERROR);

    // Attempt to add the printer.
    addDialog.shadowRoot.querySelector('.action-button').click();
    flush();

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled('getPrinterInfo');
    // The general error should be showing.
    assertTrue(!!addDialog.errorText_);
    const generalErrorElement =
        addDialog.shadowRoot.querySelector('printer-dialog-error');
    assertFalse(generalErrorElement.shadowRoot.querySelector('#error-container')
                    .hidden);
  });

  /**
   * Test that when getPrinterInfo fails for an unreachable printer, the
   printer
   * address field is marked as invalid.
   */
  test('GetPrinterInfoFailsUnreachableError', async () => {
    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();

    fillAddManuallyDialog(addDialog);

    // Make the getPrinterInfo fail for an unreachable printer.
    cupsPrintersBrowserProxy.setGetPrinterInfoResult(
        PrinterSetupResult.PRINTER_UNREACHABLE);

    // Attempt to add the printer.
    addDialog.shadowRoot.querySelector('.action-button').click();
    flush();

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled('getPrinterInfo');

    // The printer address input should be marked as invalid.
    assertTrue(
        addDialog.shadowRoot.querySelector('#printerAddressInput').invalid);
  });


  /**
   * Test that getModels isn't called with a blank query.
   */
  test('NoBlankQueries', async () => {
    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);

    // Verify that getCupsPrinterModelList is not called.
    cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList')
        .then(function(manufacturer) {
          assertNotReached(
              'No manufacturer was selected.  Unexpected model request.');
        });

    cupsPrintersBrowserProxy.manufacturers =
        ['ManufacturerA', 'ManufacturerB', 'Chromites'];
    addDialog.shadowRoot.querySelector('.action-button').click();
    flush();

    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const modelDialog = dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog');

    // Manufacturer dialog has been rendered and the model list was not
    // requested.  We're done.
    assertTrue(!!modelDialog);
  });

  /**
   * Test that dialog cancellation is logged from the manufacturer screen for
   * IPP printers.
   */
  test('LogDialogCancelledIpp', async () => {
    const makeAndModel = 'Printer Make And Model';
    // Start on add manual dialog.
    dialog.dispatchEvent(new CustomEvent('open-manually-add-printer-dialog'));
    flush();

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
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    clickAddButton(addDialog);

    // Click cancel on the manufacturer dialog when it shows up then verify
    // cancel was called with the appropriate parameters.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    flush();
    // Cancel setup with the cancel button.
    clickCancelButton(dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog'));
    const printer =
        await cupsPrintersBrowserProxy.whenCalled('cancelPrinterSetUp');
    assertTrue(!!printer, 'New printer is null');
    assertEquals(makeAndModel, printer.printerMakeAndModel);
  });

  /**
   * Test that we are checking if a printer model has an EULA upon a model
   * change.
   */
  test('getEulaUrlGetsCalledOnModelChange', async () => {
    // Start in add manual dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);

    addDialog.shadowRoot.querySelector('.action-button').click();
    flush();

    const expectedEulaLink = 'chrome://os-credits/#google';
    const expectedManufacturer = 'Google';
    const expectedModel = 'printer';
    const expectedModel2 = 'newPrinter';
    const expectedModel3 = 'newPrinter2';

    let modelDialog = null;
    let urlElement = null;
    let modelDropdown = null;

    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    modelDialog = dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!modelDialog);

    urlElement = modelDialog.shadowRoot.querySelector('#eulaUrl');
    // Check that the EULA text is not shown.
    assertTrue(urlElement.hidden);

    cupsPrintersBrowserProxy.setEulaUrl(expectedEulaLink);

    modelDialog.shadowRoot.querySelector('#manufacturerDropdown').value =
        expectedManufacturer;
    modelDropdown = modelDialog.shadowRoot.querySelector('#modelDropdown');
    modelDropdown.value = expectedModel;
    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);

    // Check that the EULA text is shown.
    assertFalse(urlElement.hidden);
    assertEquals(expectedEulaLink, urlElement.querySelector('a').href);

    resetGetEulaUrl(cupsPrintersBrowserProxy, '' /* eulaUrl */);

    // Change ppdModel and expect |getEulaUrl| to be called again.
    modelDropdown.value = expectedModel2;
    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel2);

    // Check that the EULA text is hidden.
    assertTrue(urlElement.hidden);

    resetGetEulaUrl(cupsPrintersBrowserProxy, expectedEulaLink);

    // Change ppdModel and expect |getEulaUrl| to be called again.
    modelDropdown.value = expectedModel3;
    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel3);
    assertFalse(urlElement.hidden);
    assertEquals(expectedEulaLink, urlElement.querySelector('a').href);
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled after
   * clicking it.
   */
  test('AddButtonDisabledAfterClicking', async () => {
    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);
    clickAddButton(addDialog);
    flush();

    // Click the add button on the manufacturer dialog and then verify it is
    // disabled.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const manufacturerDialog = dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!manufacturerDialog);

    // Populate the manufacturer and model fields to enable the add
    // button.
    manufacturerDialog.shadowRoot.querySelector('#manufacturerDropdown').value =
        'make';
    manufacturerDialog.shadowRoot.querySelector('#modelDropdown').value =
        'model';

    const addButton =
        manufacturerDialog.shadowRoot.querySelector('#addPrinterButton');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
    addButton.click();
    assertTrue(addButton.disabled);
  });

  /**
   * The following tests check that clicking Enter button on the keyboard
   from
   * each input text field on the add-printer-manually-dialog will advance to
   * the next dialog.
   */
  test('PressEnterInPrinterNameInput', async () => {
    mockAddPrinterInputKeyboardPress('#printerNameInput');

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertTrue(!!dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertTrue(dialog.showManufacturerDialog_);
    assertFalse(dialog.showManuallyAddDialog_);
  });

  test('PressEnterInPrinterAddressInput', async () => {
    mockAddPrinterInputKeyboardPress('#printerAddressInput');

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertFalse(
        !!dialog.shadowRoot.querySelector('add-printer-configuring-dialog'));
    assertTrue(dialog.showManufacturerDialog_);
    assertFalse(dialog.showManuallyAddDialog_);
  });

  test('PressEnterInPrinterQueueInput', async () => {
    mockAddPrinterInputKeyboardPress('#printerQueueInput');

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertTrue(!!dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertTrue(dialog.showManufacturerDialog_);
    assertFalse(dialog.showManuallyAddDialog_);
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled when the
   * manufacturer or model dropdown has an incorrect value.
   */
  test('AddButtonDisabledAfterClicking', async () => {
    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);
    clickAddButton(addDialog);
    flush();

    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const manufacturerDialog = dialog.shadowRoot.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!manufacturerDialog);

    const manufacturerDropdown =
        manufacturerDialog.shadowRoot.querySelector('#manufacturerDropdown');
    const modelDropdown =
        manufacturerDialog.shadowRoot.querySelector('#modelDropdown');
    const addButton =
        manufacturerDialog.shadowRoot.querySelector('#addPrinterButton');

    // Set the starting values for manufacturer and model dropdown.
    manufacturerDropdown.value = 'make';
    modelDropdown.value = 'model';
    assertFalse(addButton.disabled);

    // Mimic typing in random input. Make sure the Add button becomes
    // disabled.
    manufacturerDropdown.shadowRoot.querySelector('#search').value =
        'hlrRkJQkNsh';
    manufacturerDropdown.shadowRoot.querySelector('#search').dispatchEvent(
        new CustomEvent('input'));
    assertTrue(addButton.disabled);

    // Then mimic typing in the original value to re-enable the Add
    // button.
    manufacturerDropdown.shadowRoot.querySelector('#search').value = 'make';
    manufacturerDropdown.shadowRoot.querySelector('#search').dispatchEvent(
        new CustomEvent('input'));
    assertFalse(addButton.disabled);

    // Mimic typing in random input. Make sure the Add button becomes
    // disabled.
    modelDropdown.shadowRoot.querySelector('#search').value = 'hlrRkJQkNsh';
    modelDropdown.shadowRoot.querySelector('#search').dispatchEvent(
        new CustomEvent('input'));
    assertTrue(addButton.disabled);

    // Then mimic typing in the original value to re-enable the Add
    // button.
    modelDropdown.shadowRoot.querySelector('#search').value = 'model';
    modelDropdown.shadowRoot.querySelector('#search').dispatchEvent(
        new CustomEvent('input'));
    assertFalse(addButton.disabled);
  });

  test('Queue input is hidden when protocol is App Socket', async () => {
    const addDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    let printerQueueInput =
        addDialog.shadowRoot.querySelector('#printerQueueInput');
    const select = addDialog.shadowRoot.querySelector('select');
    assertTrue(!!printerQueueInput);

    select.value = 'socket';
    select.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    flush();

    printerQueueInput =
        addDialog.shadowRoot.querySelector('#printerQueueInput');
    assertFalse(!!printerQueueInput);

    select.value = 'http';
    select.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    flush();

    printerQueueInput =
        addDialog.shadowRoot.querySelector('#printerQueueInput');
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

  /** @type {!NetworkStateProperties|undefined} */
  let wifi1;

  setup(async () => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    // Simulate internet connection.
    wifi1 = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = ConnectionStateType.kOnline;

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    page.onActiveNetworksChanged([wifi1]);
    return flushTasks();
  });

  teardown(() => {
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
    const saveButton = dialog.shadowRoot.querySelector('.action-button');
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
    const cancelButton = dialog.shadowRoot.querySelector('.cancel-button');
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
    page.activePrinter = createCupsPrinterInfo(name, address, id);
    page.activePrinter.printerPpdReference.autoconf = autoconf;
    page.activePrinter.printerProtocol = protocol;
    page.activePrinter.printServerUri = serverAddress;
    cupsPrintersBrowserProxy.printerPpdMakeModel = {
      ppdManufacturer: manufacturer,
      ppdModel: model,
    };
    // Trigger the edit dialog to open.
    page.fire('edit-cups-printer-details');
    flush();
    dialog = page.shadowRoot.querySelector('settings-cups-edit-printer-dialog');
    // This proxy function gets called whenever the edit dialog is initialized.
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList');
  }

  /**
   * Test that USB printers can be edited.
   */
  test('USBPrinterCanBeEdited', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'usb', /*serverAddress=*/ '');
    // Assert that the protocol is USB.
    assertEquals('usb', dialog.shadowRoot.querySelector('.md-select').value);

    // Edit the printer name.
    const nameField = dialog.shadowRoot.querySelector('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';
    nameField.dispatchEvent(new CustomEvent('input'));

    // Assert that the "Save" button is enabled.
    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that the save button is disabled when the printer address or name is
   * invalid.
   */
  test('EditPrinter', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    assertTrue(!!dialog.shadowRoot.querySelector('#printerName'));
    assertTrue(!!dialog.shadowRoot.querySelector('#printerAddress'));

    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    // Change printer name to something valid.
    const printerName = dialog.$.printerName;
    printerName.value = 'new printer name';
    printerName.dispatchEvent(new CustomEvent('input'));
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

  /**
   * Test that closing the dialog does not persist the edits.
   */
  test('CloseEditDialogDoesNotModifyActivePrinter', async () => {
    const expectedName = 'Test Printer';
    const expectedAddress = '1.1.1.1';
    const expectedId = 'ID1';
    const expectedProtocol = 'ipp';
    await initializeAndOpenEditDialog(
        /*name=*/ expectedName, /*address=*/ expectedAddress,
        /*id=*/ expectedId, /*autoconf=*/ false,
        /*manufacturer=*/ 'make', /*model=*/ 'model',
        /*protocol=*/ expectedProtocol, /*serverAddress=*/ '');
    const nameField = dialog.shadowRoot.querySelector('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer name';

    const addressField = dialog.shadowRoot.querySelector('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = '9.9.9.9';

    const protocolField = dialog.shadowRoot.querySelector('.md-select');
    assertTrue(!!protocolField);
    protocolField.value = 'http';

    clickCancelButton(dialog);

    // Assert that activePrinter properties were not changed.
    assertEquals(expectedName, dialog.activePrinter.printerName);
    assertEquals(expectedAddress, dialog.activePrinter.printerAddress);
    assertEquals(expectedProtocol, dialog.activePrinter.printerProtocol);
  });

  /**
   * Test that editing the name field results in properly saving the new name.
   */
  test('TestEditNameAndSave', async () => {
    const expectedName = 'editedName';
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const nameField = dialog.shadowRoot.querySelector('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = expectedName;

    flush();
    clickSaveButton(dialog);
    await cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
    assertEquals(expectedName, dialog.activePrinter.printerName);
  });

  /**
   * Test that editing various fields results in properly saving the new
   * changes.
   */
  test('TestEditFieldsAndSave', async () => {
    const expectedAddress = '9.9.9.9';
    const expectedQueue = 'editedQueue';
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');

    // Editing more than just the printer name requires reconfiguring the
    // printer.
    const addressField = dialog.shadowRoot.querySelector('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = expectedAddress;

    const queueField = dialog.shadowRoot.querySelector('#printerQueue');
    assertTrue(!!queueField);
    queueField.value = expectedQueue;

    clickSaveButton(dialog);
    await cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
    assertEquals(expectedAddress, dialog.activePrinter.printerAddress);
    assertEquals(expectedQueue, dialog.activePrinter.printerQueue);
  });

  /**
   * Test that editing an autoconf printer saves correctly.
   */
  test('TestEditAutoConfFieldsAndSave', async () => {
    const expectedAddress = '9.9.9.9';
    const expectedQueue = 'editedQueue';
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ true, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    // Editing more than just the printer name requires reconfiguring the
    // printer.
    const addressField = dialog.shadowRoot.querySelector('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = expectedAddress;

    const queueField = dialog.shadowRoot.querySelector('#printerQueue');
    assertTrue(!!queueField);
    queueField.value = expectedQueue;

    clickSaveButton(dialog);
    await cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
    assertEquals(expectedAddress, dialog.activePrinter.printerAddress);
    assertEquals(expectedQueue, dialog.activePrinter.printerQueue);
  });

  /**
   * Test that non-autoconf printers can select the make and model dropdowns.
   */
  test('TestNonAutoConfPrintersCanSelectManufactureAndModel', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    // Assert that the manufacturer and model drop-downs are shown.
    assertFalse(dialog.shadowRoot.querySelector('#makeAndModelSection').hidden);
  });

  /**
   * Test that autoconf printers cannot select their make/model
   */
  test('TestAutoConfPrintersCannotSelectManufactureAndModel', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ true, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    // Assert that the manufacturer and model drop-downs are hidden.
    assertTrue(!dialog.shadowRoot.querySelector('#makeAndModelSection').if);
  });

  /**
   * Test that changing the name enables the save button.
   */
  test('TestChangingNameEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const nameField = dialog.shadowRoot.querySelector('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';
    nameField.dispatchEvent(new CustomEvent('input'));

    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that changing the address enables the save button.
   */
  test('TestChangingAddressEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const addressField = dialog.shadowRoot.querySelector('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = 'newAddress:789';
    addressField.dispatchEvent(new CustomEvent('input'));

    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that changing the queue enables the save button.
   */
  test('TestChangingQueueEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const queueField = dialog.shadowRoot.querySelector('#printerQueue');
    assertTrue(!!queueField);
    queueField.value = 'newqueueinfo';
    queueField.dispatchEvent(new CustomEvent('input'));

    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that changing the protocol enables the save button.
   */
  test('TestChangingProtocolEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const dropDown = dialog.shadowRoot.querySelector('.md-select');
    dropDown.value = 'http';
    dropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});
    flush();
    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that changing the model enables the save button.
   */
  test('TestChangingModelEnablesSaveButton', async () => {
    let saveButton = null;

    cupsPrintersBrowserProxy.manufacturers = {
      success: true,
      manufacturers: ['HP'],
    };
    cupsPrintersBrowserProxy.models = {success: true, models: ['HP 910']};
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const makeDropDown =
        dialog.shadowRoot.querySelector('#printerPPDManufacturer');
    makeDropDown.value = 'HP';
    makeDropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});

    await cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList');
    // Saving is disabled until a model is selected.
    assertTrue(saveButton.disabled);

    const modelDropDown = dialog.shadowRoot.querySelector('#printerPPDModel');
    modelDropDown.value = 'HP 910';
    modelDropDown.dispatchEvent(new CustomEvent('change'), {'bubbles': true});

    flush();
    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that we are checking if a printer model has an EULA upon a model
   * change.
   */
  test('getEulaUrlGetsCalledOnModelChange', async () => {
    const eulaLink = 'google.com';
    const expectedManufacturer = 'Google';
    const expectedModel = 'model';
    const expectedModel2 = 'newModel';
    const expectedModel3 = 'newModel2';

    let modelDropdown = null;
    let urlElement = null;
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    urlElement = dialog.shadowRoot.querySelector('#eulaUrl');
    // Check that the EULA text is hidden.
    assertTrue(urlElement.hidden);

    // 'getEulaUrl' is called as part of the initialization of the dialog,
    // so we have to reset the resolver before the next call.
    resetGetEulaUrl(cupsPrintersBrowserProxy, eulaLink);

    dialog.shadowRoot.querySelector('#printerPPDManufacturer').value =
        expectedManufacturer;
    modelDropdown = dialog.shadowRoot.querySelector('#printerPPDModel');
    modelDropdown.value = expectedModel;

    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);

    // Check that the EULA text is shown.
    assertFalse(urlElement.hidden);

    resetGetEulaUrl(cupsPrintersBrowserProxy, /*eulaUrl=*/ '');

    // Change ppdModel and expect |getEulaUrl| to be called again.
    modelDropdown.value = expectedModel2;
    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel2);

    // Check that the EULA text is hidden.
    assertTrue(urlElement.hidden);

    resetGetEulaUrl(cupsPrintersBrowserProxy, eulaLink);

    // Change ppdModel and expect |getEulaUrl| to be called again.
    modelDropdown.value = expectedModel3;
    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel3);

    // Check that the EULA text is shown again.
    assertFalse(urlElement.hidden);
  });

  /**
   * Test that editing the name is still supported when offline.
   */
  test('OfflineEdit', async () => {
    // Simulate connecting to a network with no internet connection.
    wifi1.connectionState = ConnectionStateType.kConnected;
    page.onActiveNetworksChanged([wifi1]);
    flush();
    const expectedName = 'editedName';
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const nameField = dialog.shadowRoot.querySelector('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = expectedName;
    nameField.dispatchEvent(new CustomEvent('input'));

    flush();

    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertFalse(saveButton.disabled);

    clickSaveButton(dialog);
    await cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
    assertEquals(expectedName, dialog.activePrinter.printerName);
  });

  test('PrintServerPrinterEdit', async () => {
    const expectedName = 'edited name';
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ true, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp',
        /*serverAddress=*/ 'ipp://192.168.1.1:631');

    // Verify the only the name field is not disabled.
    assertTrue(dialog.shadowRoot.querySelector('#printerAddress').disabled);
    assertTrue(dialog.shadowRoot.querySelector('.md-select').disabled);
    assertTrue(dialog.shadowRoot.querySelector('#printerQueue').disabled);

    const nameField = dialog.shadowRoot.querySelector('.printer-name-input');
    assertTrue(!!nameField);
    assertFalse(nameField.disabled);

    nameField.value = expectedName;
    nameField.dispatchEvent(new CustomEvent('input'));

    flush();

    const saveButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!saveButton);
    assertFalse(saveButton.disabled);

    clickSaveButton(dialog);
    await cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
    assertEquals(expectedName, dialog.activePrinter.printerName);
  });
});

suite('PrintServerTests', function() {
  let page = null;
  let dialog = null;

  /** @type {?CupsPrintersEntryManager} */
  let entryManager = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;


  setup(async () => {
    entryManager = CupsPrintersEntryManager.getInstance();
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*automaticPrinters=*/[],
        /*discoveredPrinters=*/[], /*printServerPrinters=*/[]);

    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    dialog = page.shadowRoot.querySelector('settings-cups-add-printer-dialog');
    assertTrue(!!dialog);

    return flushTasks();
  });

  teardown(() => {
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
    dialog = page.shadowRoot.querySelector('settings-cups-add-printer-dialog');
    return dialog.shadowRoot.querySelector('add-print-server-dialog');
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
    flush();

    const addPrinterDialog =
        dialog.shadowRoot.querySelector('add-printer-manually-dialog');
    // Switch to Add print server dialog.
    addPrinterDialog.shadowRoot.querySelector('#print-server-button').click();
    flush();
    const printServerDialog =
        dialog.shadowRoot.querySelector('add-print-server-dialog');
    assertTrue(!!printServerDialog);

    flush();
    cupsPrintersBrowserProxy.setQueryPrintServerResult(error);
    return flushTasks().then(() => {
      // Fill dialog with the server address.
      const address = printServerDialog.shadowRoot.querySelector(
          '#printServerAddressInput');
      assertTrue(!!address);
      address.value = address;

      // Add the print server.
      const button =
          printServerDialog.shadowRoot.querySelector('.action-button');
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
    const dialogError =
        printServerDialog.shadowRoot.querySelector('#server-dialog-error');
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
    const toast = page.shadowRoot.querySelector('#printServerErrorToast');
    assertTrue(toast.open);
    assertEquals(
        loadTimeData.getStringF(expectedMessage, numPrinters),
        toast.textContent.trim());
  }

  test('AddPrintServerIsSuccessful', async () => {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{!CupsPrintServerPrintersInfo} */ ({
          printerList: [
            createCupsPrinterInfo('nameA', 'serverAddress', 'idA'),
            createCupsPrinterInfo('nameB', 'serverAddress', 'idB'),
          ],
        });
    await addPrintServer('serverAddress', PrintServerResult.NO_ERRORS);
    flush();
    verifyToastMessage('printServerFoundManyPrinters', /*numPrinters=*/ 2);
    assertEquals(2, entryManager.printServerPrinters.length);
  });

  test('HandleDuplicateQueries', async () => {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{!CupsPrintServerPrintersInfo} */ ({
          printerList: [
            createCupsPrinterInfo('nameA', 'serverAddress', 'idA'),
            createCupsPrinterInfo('nameB', 'serverAddress', 'idB'),
          ],
        });

    await flushTasks();
    // Simulate that a print server was queried previously.
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*nearbyPrinters=*/[],
        /*discoveredPrinters=*/[], [
          createPrinterListEntry(
              'nameA', 'serverAddress', 'idA', PrinterType.PRINTSERVER),
          createPrinterListEntry(
              'nameB', 'serverAddress', 'idB', PrinterType.PRINTSERVER),
        ]);
    flush();
    assertEquals(2, entryManager.printServerPrinters.length);

    // This will attempt to add duplicate print server printers.
    // Matching printerId's are considered duplicates.
    await addPrintServer('serverAddress', PrintServerResult.NO_ERRORS);
    flush();

    verifyToastMessage('printServerFoundManyPrinters', /*numPrinters=*/ 2);
    // Assert that adding the same print server results in no new printers
    // added to the entry manager.
    assertEquals(2, entryManager.printServerPrinters.length);
    const nearbyPrintersElement =
        page.shadowRoot.querySelector('settings-cups-nearby-printers');
    assertEquals(2, nearbyPrintersElement.nearbyPrinters.length);
  });

  test('HandleDuplicateSavedPrinters', async () => {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({
          printerList: [
            createCupsPrinterInfo('nameA', 'serverAddress', 'idA'),
            createCupsPrinterInfo('nameB', 'serverAddress', 'idB'),
          ],
        });

    await flushTasks();
    // Simulate that a print server was queried previously.
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*nearbyPrinters=*/[],
        /*discoveredPrinters=*/[], [
          createPrinterListEntry(
              'nameA', 'serverAddress', 'idA', PrinterType.PRINTSERVER),
          createPrinterListEntry(
              'nameB', 'serverAddress', 'idB', PrinterType.PRINTSERVER),
        ]);
    flush();
    assertEquals(2, entryManager.printServerPrinters.length);

    // Simulate adding a saved printer.
    entryManager.setSavedPrintersList([createPrinterListEntry(
        'nameA', 'serverAddress', 'idA', PrinterType.SAVED)]);
    flush();

    // Simulate the underlying model changes. Nearby printers are also
    // updated after changes to saved printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', /*automaticPrinter=*/[],
        /*discoveredPrinters=*/[]);
    await flushTasks();

    // Verify that we now only have 1 printer in print server printers
    // list.
    assertEquals(1, entryManager.printServerPrinters.length);
    const nearbyPrintersElement =
        page.shadowRoot.querySelector('settings-cups-nearby-printers');
    assertTrue(!!nearbyPrintersElement);
    assertEquals(1, nearbyPrintersElement.nearbyPrinters.length);
    // Verify we correctly removed the duplicate printer, 'idA', since
    // it exists in the saved printer list. Expect only 'idB' in
    // the print server printers list.
    assertEquals(
        'idB', entryManager.printServerPrinters[0].printerInfo.printerId);
  });

  test('AddPrintServerAddressError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    await addPrintServer('serverAddress', PrintServerResult.INCORRECT_URL);
    flush();
    const printServerDialog = getPrintServerDialog(page);
    // Assert that the dialog did not close on errors.
    assertTrue(!!printServerDialog);
    // Assert that the address input field is invalid.
    assertTrue(
        printServerDialog.shadowRoot.querySelector('#printServerAddressInput')
            .invalid);
  });

  test('AddPrintServerConnectionError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    await addPrintServer('serverAddress', PrintServerResult.CONNECTION_ERROR);
    flush();
    verifyErrorMessage('printServerConnectionError');
  });

  test('AddPrintServerReachableServerButIppResponseError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    await addPrintServer(
        'serverAddress', PrintServerResult.CANNOT_PARSE_IPP_RESPONSE);
    flush();
    verifyErrorMessage('printServerConfigurationErrorMessage');
  });

  test('AddPrintServerReachableServerButHttpResponseError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters =
        /** @type{} CupsPrintServerPrintersInfo*/ ({printerList: []});
    await addPrintServer('serverAddress', PrintServerResult.HTTP_ERROR);
    flush();
    verifyErrorMessage('printServerConfigurationErrorMessage');
  });
});
