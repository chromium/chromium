// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AddPrinterManuallyDialogElement, AddPrinterManufacturerModelDialogElement, CupsPrintersBrowserProxyImpl, PrinterSetupResult, SettingsCupsAddPrinterDialogElement, SettingsCupsEditPrinterDialogElement, SettingsCupsPrintersElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrSearchableDropDownElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {DomIf, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {createCupsPrinterInfo} from './cups_printer_test_utils.js';
import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.js';

/*
 * Helper function that waits for |getEulaUrl| to get called and then verifies
 * its arguments.
 */
async function verifyGetEulaUrlWasCalled(
    cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy,
    expectedManufacturer: string, expectedModel: string): Promise<void> {
  const args = await cupsPrintersBrowserProxy.whenCalled('getEulaUrl');
  assertEquals(expectedManufacturer, args[0]);  // ppdManufacturer
  assertEquals(expectedModel, args[1]);         // ppdModel
}

/*
 * Helper function that resets the resolver for |getEulaUrl| and sets the new
 * EULA URL.
 */
function resetGetEulaUrl(
    cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy,
    eulaUrl: string): void {
  cupsPrintersBrowserProxy.resetResolver('getEulaUrl');
  cupsPrintersBrowserProxy.setEulaUrl(eulaUrl);
}

suite('CupsAddPrinterDialogTests', () => {
  let page: SettingsCupsPrintersElement;
  let dialog: SettingsCupsAddPrinterDialogElement;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;

  function fillAddManuallyDialog(addDialog: AddPrinterManuallyDialogElement):
      void {
    const name = addDialog.shadowRoot!.querySelector<HTMLInputElement>(
        '#printerNameInput');
    const address = addDialog.shadowRoot!.querySelector<HTMLInputElement>(
        '#printerAddressInput');

    assertTrue(!!name);
    name.value = 'Test printer';

    assertTrue(!!address);
    address.value = '127.0.0.1';

    const addButton = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#addPrinterButton');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
  }

  function clickAddButton(dialog: AddPrinterManuallyDialogElement) {
    assertTrue(!!dialog, 'Dialog is null for add');
    const addButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!addButton, 'Button is null');
    addButton.click();
  }

  function clickCancelButton(dialog: AddPrinterManufacturerModelDialogElement) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
    assertTrue(!!cancelButton, 'Button is null');
    cancelButton.click();
  }

  function canAddPrinter(
      dialog: AddPrinterManuallyDialogElement, name: string, address: string) {
    dialog.newPrinter.printerName = name;
    dialog.newPrinter.printerAddress = address;
    return dialog['canAddPrinter_']();
  }

  function mockAddPrinterInputKeyboardPress(crInputId: string) {
    // Start in add manual dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);

    // Test that pressing Enter before all the fields are populated does not
    // advance to the next dialog.
    const input = addDialog.shadowRoot!.querySelector(crInputId);
    assertTrue(!!input);
    keyEventOn(input, 'keypress', /*keycode=*/ 13, [], 'Enter');
    flush();

    assertNull(dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertFalse(dialog.get('showManufacturerDialog_'));
    assertTrue(dialog.get('showManuallyAddDialog_'));

    // Add valid input into the dialog
    fillAddManuallyDialog(addDialog);

    // Test that key press on random key while in input field is not accepted as
    // as valid Enter press.
    keyEventOn(input, 'keypress', /*keycode=*/ 16, [], 'Shift');
    flush();

    assertNull(dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertFalse(dialog.get('showManufacturerDialog_'));
    assertTrue(dialog.get('showManuallyAddDialog_'));

    // Now test Enter press with valid input.
    keyEventOn(input, 'keypress', /*keycode=*/ 13, [], 'Enter');
    flush();
  }

  setup(() => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    page = document.createElement('settings-cups-printers');
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.set('enableUpdatedUi_', false);
    document.body.appendChild(page);
    assertTrue(!!page);
    const element =
        page.shadowRoot!.querySelector('settings-cups-add-printer-dialog');
    assertTrue(!!element);
    dialog = element;

    dialog.open();
    return flushTasks();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog.remove();
  });

  test('ValidIPV4', () => {
    const dialog = document.createElement('add-printer-manually-dialog');
    assertTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1'));
  });

  test('ValidIPV4WithPort', () => {
    const dialog = document.createElement('add-printer-manually-dialog');
    assertTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1:1234'));
  });

  test('ValidIPV6', () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    // Test the full ipv6 address scheme.
    assertTrue(canAddPrinter(dialog, 'Test printer', '1:2:a3:ff4:5:6:7:8'));

    // Test the shorthand prefix scheme.
    assertTrue(canAddPrinter(dialog, 'Test printer', '::255'));

    // Test the shorthand suffix scheme.
    assertTrue(canAddPrinter(dialog, 'Test printer', '1::'));
  });

  test('ValidIPV6WithPort', () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertTrue(canAddPrinter(dialog, 'Test printer', '[1:2:aa2:4]:12'));
    assertTrue(canAddPrinter(dialog, 'Test printer', '[::255]:54'));
    assertTrue(canAddPrinter(dialog, 'Test printer', '[1::]:7899'));
  });

  test('InvalidIPV6', () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:4:5:6:7:8:9'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:aa:a1245:2'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:za:2'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1:::22'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '1::2::3'));
  });

  test('ValidHostname', () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertTrue(canAddPrinter(dialog, 'Test printer', 'hello-world.com'));
    assertTrue(canAddPrinter(dialog, 'Test printer', 'hello.world.com:12345'));
  });

  test('InvalidHostname', () => {
    const dialog = document.createElement('add-printer-manually-dialog');

    assertFalse(canAddPrinter(dialog, 'Test printer', 'helloworld!123.com'));
    assertFalse(canAddPrinter(dialog, 'Test printer', 'helloworld123-.com'));
    assertFalse(canAddPrinter(dialog, 'Test printer', '-helloworld123.com'));
  });

  test('AddPrinterManuallySuccess', async () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();

    // Fill the printer dialog with default printer details.
    fillAddManuallyDialog(addDialog);

    // Make the PPD resolved true so the printer add is successful.
    cupsPrintersBrowserProxy.setPpdReferenceResolved(true);

    // Attempt to add the printer.
    const button = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    button.click();
    await cupsPrintersBrowserProxy.whenCalled('addCupsPrinter');
    await flushTasks();

    // Verify the dialog closes after successful add.
    const addPrinterDialog =
        addDialog.shadowRoot!.querySelector('add-printer-dialog');
    assertTrue(!!addPrinterDialog);
    const crDialog =
        addPrinterDialog.shadowRoot!.querySelector<CrDialogElement>(
            'cr-dialog');
    assertTrue(!!crDialog);
    assertFalse(crDialog.open);

    // Record the success to metrics.
    assertEquals(
        1,
        fakeMetricsPrivate.countBoolean(
            'Printing.CUPS.AddPrinterManuallyResult', true));
  });

  /**
   * Test that clicking on Add opens the model select page.
   */
  test('ValidAddOpensModelSelection', async () => {
    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);

    const button = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    button.click();
    flush();

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertTrue(!!dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog'));

    assertTrue(dialog.get('showManufacturerDialog_'));
    assertFalse(dialog.get('showManuallyAddDialog_'));
  });

  /**
   * Test that when getPrinterInfo fails for a generic reason, the general error
   * message is shown.
   */
  test('GetPrinterInfoFailsGeneralError', async () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();

    fillAddManuallyDialog(addDialog);

    // Make the getPrinterInfo fail for a generic error.
    cupsPrintersBrowserProxy.setGetPrinterInfoResult(
        PrinterSetupResult.FATAL_ERROR);

    // Attempt to add the printer.
    const button = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    button.click();
    flush();

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled('getPrinterInfo');
    // The general error should be showing.
    assertTrue(!!addDialog.get('errorText_'));
    const generalErrorElement =
        addDialog.shadowRoot!.querySelector('printer-dialog-error');
    assertTrue(!!generalErrorElement);
    const errorContainer =
        generalErrorElement.shadowRoot!.querySelector<HTMLElement>(
            '#error-container');
    assertTrue(!!errorContainer);
    assertFalse(errorContainer.hidden);
    assertEquals(
        1,
        fakeMetricsPrivate.countBoolean(
            'Printing.CUPS.AddPrinterManuallyResult', false));
  });

  /**
   * Test that when getPrinterInfo fails for an unreachable printer, the
   printer
   * address field is marked as invalid.
   */
  test('GetPrinterInfoFailsUnreachableError', async () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();

    fillAddManuallyDialog(addDialog);

    // Make the getPrinterInfo fail for an unreachable printer.
    cupsPrintersBrowserProxy.setGetPrinterInfoResult(
        PrinterSetupResult.PRINTER_UNREACHABLE);

    // Attempt to add the printer.
    const button = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    button.click();
    flush();

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled('getPrinterInfo');

    // The printer address input should be marked as invalid.
    const printerAddressInput =
        addDialog.shadowRoot!.querySelector<CrInputElement>(
            '#printerAddressInput');
    assertTrue(!!printerAddressInput);
    assertTrue(printerAddressInput.invalid);
    assertEquals(
        1,
        fakeMetricsPrivate.countBoolean(
            'Printing.CUPS.AddPrinterManuallyResult', false));
  });

  /**
   * Test that getModels isn't called with a blank query.
   */
  test('NoBlankQueries', async () => {
    // Starts in add manual dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);

    cupsPrintersBrowserProxy.manufacturers = {
      success: false,
      manufacturers: ['ManufacturerA', 'ManufacturerB', 'Chromites'],
    };
    const button = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    button.click();
    flush();

    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');

    // Verify that getCupsPrinterModelList is not called.
    assertEquals(
        0, cupsPrintersBrowserProxy.getCallCount('getCupsPrinterModelsList'));

    const modelDialog = dialog.shadowRoot!.querySelector(
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
      isManaged: false,
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
      printServerUri: '',
    };

    // Seed the getPrinterInfo response.  We detect the make and model but it is
    // not an autoconf printer.
    cupsPrintersBrowserProxy.printerInfo = {
      autoconf: false,
      makeAndModel,
      ppdRefUserSuppliedPpdUrl: 'ppd url',
      ppdRefEffectiveMakeAndModel: 'Effective Make And Model',
      ppdReferenceResolved: false,
    };

    // Press the add button to advance dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    clickAddButton(addDialog);

    // Click cancel on the manufacturer dialog when it shows up then verify
    // cancel was called with the appropriate parameters.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    flush();
    // Cancel setup with the cancel button.
    const modelDialog = dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!modelDialog);
    clickCancelButton(modelDialog);
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
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);

    const button = addDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    button.click();
    flush();

    const expectedEulaLink = 'chrome://os-credits/#google';
    const expectedManufacturer = 'Google';
    const expectedModel = 'printer';
    const expectedModel2 = 'newPrinter';
    const expectedModel3 = 'newPrinter2';

    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const modelDialog = dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!modelDialog);

    const urlElement =
        modelDialog.shadowRoot!.querySelector<HTMLElement>('#eulaUrl');
    assertTrue(!!urlElement);
    // Check that the EULA text is not shown.
    assertTrue(urlElement.hidden);

    cupsPrintersBrowserProxy.setEulaUrl(expectedEulaLink);

    const manufacturerDropdown =
        modelDialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#manufacturerDropdown');
    assertTrue(!!manufacturerDropdown);
    manufacturerDropdown.value = expectedManufacturer;
    const modelDropdown =
        modelDialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#modelDropdown');
    assertTrue(!!modelDropdown);
    modelDropdown.value = expectedModel;
    await verifyGetEulaUrlWasCalled(
        cupsPrintersBrowserProxy, expectedManufacturer, expectedModel);

    // Check that the EULA text is shown.
    assertFalse(urlElement.hidden);
    let anchor = urlElement.querySelector('a');
    assertTrue(!!anchor);
    assertEquals(expectedEulaLink, anchor.href);

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
    anchor = urlElement.querySelector('a');
    assertTrue(!!anchor);
    assertEquals(expectedEulaLink, anchor.href);
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled after
   * clicking it.
   */
  test('AddButtonDisabledAfterClicking', async () => {
    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);
    clickAddButton(addDialog);
    flush();

    // Click the add button on the manufacturer dialog and then verify it is
    // disabled.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const manufacturerDialog = dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!manufacturerDialog);

    // Populate the manufacturer and model fields to enable the add
    // button.
    const manufacturerDropdown =
        manufacturerDialog.shadowRoot!
            .querySelector<CrSearchableDropDownElement>(
                '#manufacturerDropdown');
    assertTrue(!!manufacturerDropdown);
    manufacturerDropdown.value = 'make';
    const modelDropdown =
        manufacturerDialog.shadowRoot!
            .querySelector<CrSearchableDropDownElement>('#modelDropdown');
    assertTrue(!!modelDropdown);
    modelDropdown.value = 'model';

    const addButton =
        manufacturerDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addPrinterButton');
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
    assertTrue(!!dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertTrue(dialog.get('showManufacturerDialog_'));
    assertFalse(dialog.get('showManuallyAddDialog_'));
  });

  test('PressEnterInPrinterAddressInput', async () => {
    mockAddPrinterInputKeyboardPress('#printerAddressInput');

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertNull(
        dialog.shadowRoot!.querySelector('add-printer-configuring-dialog'));
    assertTrue(dialog.get('showManufacturerDialog_'));
    assertFalse(dialog.get('showManuallyAddDialog_'));
  });

  test('PressEnterInPrinterQueueInput', async () => {
    mockAddPrinterInputKeyboardPress('#printerQueueInput');

    // Upon rejection, show model.
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    await flushTasks();
    // Showing model selection.
    assertTrue(!!dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog'));
    assertTrue(dialog.get('showManufacturerDialog_'));
    assertFalse(dialog.get('showManuallyAddDialog_'));
  });

  /**
   * Test that the add button of the manufacturer dialog is disabled when the
   * manufacturer or model dropdown has an incorrect value.
   */
  test('AddButtonDisabledAfterClicking', async () => {
    // From the add manually dialog, click the add button to advance to the
    // manufacturer dialog.
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    flush();
    fillAddManuallyDialog(addDialog);
    clickAddButton(addDialog);
    flush();

    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const manufacturerDialog = dialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!manufacturerDialog);

    const manufacturerDropdown =
        manufacturerDialog.shadowRoot!
            .querySelector<CrSearchableDropDownElement>(
                '#manufacturerDropdown');
    const modelDropdown =
        manufacturerDialog.shadowRoot!
            .querySelector<CrSearchableDropDownElement>('#modelDropdown');
    const addButton =
        manufacturerDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addPrinterButton');

    assertTrue(!!manufacturerDropdown);
    assertTrue(!!modelDropdown);
    assertTrue(!!addButton);
    // Set the starting values for manufacturer and model dropdown.
    manufacturerDropdown.value = 'make';
    modelDropdown.value = 'model';
    assertFalse(addButton.disabled);

    // Mimic typing in random input. Make sure the Add button becomes
    // disabled.
    let searchElement =
        manufacturerDropdown.shadowRoot!.querySelector<HTMLInputElement>(
            '#search');
    assertTrue(!!searchElement);
    searchElement.value = 'hlrRkJQkNsh';
    searchElement.dispatchEvent(new CustomEvent('input'));
    assertTrue(addButton.disabled);

    // Then mimic typing in the original value to re-enable the Add
    // button.
    searchElement =
        manufacturerDropdown.shadowRoot!.querySelector<HTMLInputElement>(
            '#search');
    assertTrue(!!searchElement);
    searchElement.value = 'make';
    searchElement.dispatchEvent(new CustomEvent('input'));
    assertFalse(addButton.disabled);

    // Mimic typing in random input. Make sure the Add button becomes
    // disabled.
    searchElement =
        modelDropdown.shadowRoot!.querySelector<HTMLInputElement>('#search');
    assertTrue(!!searchElement);
    searchElement.value = 'hlrRkJQkNsh';
    searchElement.dispatchEvent(new CustomEvent('input'));
    assertTrue(addButton.disabled);

    // Then mimic typing in the original value to re-enable the Add
    // button.
    searchElement =
        modelDropdown.shadowRoot!.querySelector<HTMLInputElement>('#search');
    assertTrue(!!searchElement);
    searchElement.value = 'model';
    searchElement.dispatchEvent(new CustomEvent('input'));
    assertFalse(addButton.disabled);
  });

  test('Queue input is hidden when protocol is App Socket', () => {
    const addDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    let printerQueueInput =
        addDialog.shadowRoot!.querySelector('#printerQueueInput');
    assertTrue(!!printerQueueInput);
    const select = addDialog.shadowRoot!.querySelector('select');
    assertTrue(!!select);

    select.value = 'socket';
    select.dispatchEvent(new CustomEvent('change', {'bubbles': true}));
    flush();

    printerQueueInput =
        addDialog.shadowRoot!.querySelector('#printerQueueInput');
    assertNull(printerQueueInput);

    select.value = 'http';
    select.dispatchEvent(new CustomEvent('change', {'bubbles': true}));
    flush();

    printerQueueInput =
        addDialog.shadowRoot!.querySelector('#printerQueueInput');
    assertTrue(!!printerQueueInput);
  });
});

suite('EditPrinterDialog', () => {
  let page: SettingsCupsPrintersElement;
  let dialog: SettingsCupsEditPrinterDialogElement;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;
  let wifi1: NetworkStateProperties;

  setup(async () => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    // Simulate internet connection.
    wifi1 = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = ConnectionStateType.kOnline;

    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    page.onActiveNetworksChanged([wifi1]);
    await flushTasks();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog.remove();
  });

  function clickSaveButton(dialog: SettingsCupsEditPrinterDialogElement) {
    assertTrue(!!dialog, 'Dialog is null for save');
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    dialog.set('printerInfoChanged_', true);
    assertTrue(!!saveButton, 'Button is null');
    assertFalse(saveButton.disabled);
    saveButton.click();
  }

  function clickCancelButton(dialog: SettingsCupsEditPrinterDialogElement) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
    assertTrue(!!cancelButton, 'Button is null');
    cancelButton.click();
  }

  /**
   * Initializes a printer and sets that printer as the printer to be edited in
   * the edit dialog. Opens the edit dialog.
   */
  async function initializeAndOpenEditDialog(
      name: string, address: string, id: string, autoconf: boolean,
      manufacturer: string, model: string, protocol: string,
      serverAddress: string): Promise<void> {
    page.activePrinter = createCupsPrinterInfo(name, address, id);
    page.activePrinter.printerPpdReference.autoconf = autoconf;
    page.activePrinter.printerProtocol = protocol;
    page.activePrinter.printServerUri = serverAddress;
    cupsPrintersBrowserProxy.printerPpdMakeModel = {
      ppdManufacturer: manufacturer,
      ppdModel: model,
    };
    // Trigger the edit dialog to open.
    page.dispatchEvent(new CustomEvent('edit-cups-printer-details'));
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-edit-printer-dialog');
    assertTrue(!!element);
    dialog = element;
    // This proxy function gets called whenever the edit dialog is initialized.
    await cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList');
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
    const selectElement =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select');
    assertTrue(!!selectElement);
    assertEquals('usb', selectElement.value);

    // Edit the printer name.
    const nameField = dialog.shadowRoot!.querySelector<HTMLInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';
    nameField.dispatchEvent(new CustomEvent('input'));

    // Assert that the "Save" button is enabled.
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertFalse(saveButton.disabled);
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
    const printerName =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerName');
    const printerAddress =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerAddress');
    assertTrue(!!printerName);
    assertTrue(!!printerAddress);

    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    // Change printer name to something valid.
    printerName.value = 'new printer name';
    printerName.dispatchEvent(new CustomEvent('input'));
    assertFalse(saveButton.disabled);

    // Change printer address to something invalid.
    printerAddress.value = 'abcdef:';
    assertTrue(saveButton.disabled);

    // Change back to something valid.
    printerAddress.value = 'abcdef:1234';
    assertFalse(saveButton.disabled);

    // Change printer name to empty.
    printerName.value = '';
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
    const nameField = dialog.shadowRoot!.querySelector<HTMLInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer name';

    const addressField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = '9.9.9.9';

    const protocolField =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select');
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
    const nameField = dialog.shadowRoot!.querySelector<HTMLInputElement>(
        '.printer-name-input');
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
    const addressField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = expectedAddress;

    const queueField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerQueue');
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
    const addressField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = expectedAddress;

    const queueField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerQueue');
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
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#makeAndModelSection');
    assertTrue(!!element);
    assertFalse(element.hidden);
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
    const element =
        dialog.shadowRoot!.querySelector<DomIf>('#makeAndModelSection');
    assertTrue(!!element);
    assertTrue(!element.if);
  });

  /**
   * Test that changing the name enables the save button.
   */
  test('TestChangingNameEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const nameField = dialog.shadowRoot!.querySelector<HTMLInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';
    nameField.dispatchEvent(new CustomEvent('input'));

    assertFalse(saveButton.disabled);
  });

  /**
   * Test that changing the address enables the save button.
   */
  test('TestChangingAddressEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const addressField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = 'newAddress:789';
    addressField.dispatchEvent(new CustomEvent('input'));

    assertFalse(saveButton.disabled);
  });

  /**
   * Test that changing the queue enables the save button.
   */
  test('TestChangingQueueEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const queueField =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerQueue');
    assertTrue(!!queueField);
    queueField.value = 'newqueueinfo';
    queueField.dispatchEvent(new CustomEvent('input'));

    assertFalse(saveButton.disabled);
  });

  /**
   * Test that changing the protocol enables the save button.
   */
  test('TestChangingProtocolEnablesSaveButton', async () => {
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const dropDown =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select');
    assertTrue(!!dropDown);
    dropDown.value = 'http';
    dropDown.dispatchEvent(new CustomEvent('change', {'bubbles': true}));
    flush();
    assertFalse(saveButton.disabled);
  });

  /**
   * Test that changing the model enables the save button.
   */
  test('TestChangingModelEnablesSaveButton', async () => {
    cupsPrintersBrowserProxy.manufacturers = {
      success: true,
      manufacturers: ['HP'],
    };
    cupsPrintersBrowserProxy.models = {success: true, models: ['HP 910']};
    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertTrue(saveButton.disabled);

    const makeDropDown =
        dialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#printerPPDManufacturer');
    assertTrue(!!makeDropDown);
    makeDropDown.value = 'HP';
    makeDropDown.dispatchEvent(new CustomEvent('change', {'bubbles': true}));

    await cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList');
    // Saving is disabled until a model is selected.
    assertTrue(saveButton.disabled);

    const modelDropDown =
        dialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#printerPPDModel');
    assertTrue(!!modelDropDown);
    modelDropDown.value = 'HP 910';
    modelDropDown.dispatchEvent(new CustomEvent('change', {'bubbles': true}));

    flush();
    assertFalse(saveButton.disabled);
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

    await initializeAndOpenEditDialog(
        /*name=*/ 'name', /*address=*/ 'address', /*id=*/ 'id',
        /*autoconf=*/ false, /*manufacturer=*/ 'make',
        /*model=*/ 'model', /*protocol=*/ 'ipp', /*serverAddress=*/ '');
    const urlElement =
        dialog.shadowRoot!.querySelector<HTMLElement>('#eulaUrl');
    assertTrue(!!urlElement);
    // Check that the EULA text is hidden.
    assertTrue(urlElement.hidden);

    // 'getEulaUrl' is called as part of the initialization of the dialog,
    // so we have to reset the resolver before the next call.
    resetGetEulaUrl(cupsPrintersBrowserProxy, eulaLink);

    const makeDropDown =
        dialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#printerPPDManufacturer');
    assertTrue(!!makeDropDown);
    makeDropDown.value = expectedManufacturer;
    const modelDropdown =
        dialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#printerPPDModel');
    assertTrue(!!modelDropdown);
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
    const nameField = dialog.shadowRoot!.querySelector<HTMLInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = expectedName;
    nameField.dispatchEvent(new CustomEvent('input'));

    flush();

    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
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
    const printerAddress =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerAddress');
    assertTrue(!!printerAddress);
    assertTrue(printerAddress.disabled);
    const selectElement =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select');
    assertTrue(!!selectElement);
    assertTrue(selectElement.disabled);
    const printerQueue =
        dialog.shadowRoot!.querySelector<HTMLInputElement>('#printerQueue');
    assertTrue(!!printerQueue);
    assertTrue(printerQueue.disabled);

    const nameField = dialog.shadowRoot!.querySelector<HTMLInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    assertFalse(nameField.disabled);

    nameField.value = expectedName;
    nameField.dispatchEvent(new CustomEvent('input'));

    flush();

    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertFalse(saveButton.disabled);

    clickSaveButton(dialog);
    await cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
    assertEquals(expectedName, dialog.activePrinter.printerName);
  });
});
