// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceImpl, DuplexMode, NativeInitialSettings, NativeLayerImpl, PluginProxyImpl, PrintPreviewAppElement} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate, getCloudDestination} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';


const print_preview_app_test = {
  suiteName: 'PrintPreviewAppTest',
  TestNames: {
    PrintToGoogleDrive: 'print to google drive',
    PrintPresets: 'print presets',
    DestinationsManaged: 'destinations managed',
    HeaderFooterManaged: 'header footer managed',
    CssBackgroundManaged: 'css background managed',
    SheetsManaged: 'sheets managed'
  },
};

Object.assign(window, {print_preview_app_test: print_preview_app_test});

suite(print_preview_app_test.suiteName, function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

  let cloudPrintInterface: CloudPrintInterfaceStub;

  let pluginProxy: TestPluginProxy;

  const initialSettings: NativeInitialSettings = {
    isInKioskAutoPrintMode: false,
    isInAppKioskMode: false,
    thousandsDelimiter: ',',
    decimalDelimiter: '.',
    unitType: 1,
    previewModifiable: true,
    documentTitle: 'DocumentABC123',
    documentHasSelection: false,
    shouldPrintSelectionOnly: false,
    printerName: 'FooDevice',
    serializedAppStateStr: null,
    serializedDefaultDestinationSelectionRulesStr: null,
    pdfPrinterDisabled: false,
    destinationsManaged: false,
    cloudPrintURL: 'cloudprint URL',
    previewIsFromArc: false,
    uiLocale: 'en-us',
  };

  /**
   * Set the native layer initial settings and attach the print preview app to
   * the DOM.
   * @return Returns a promise that resolves when the 'getPreview'
   *     method is called by the preview area contained inside the app.
   */
  function initialize(): Promise<void> {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    return nativeLayer.whenCalled('getPreview');
  }

  setup(function() {
    // Stub out the native layer, the cloud print interface, and the plugin.
    document.body.innerHTML = '';
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="chromeos_ash or chromeos_lacros">
    setNativeLayerCrosInstance();
    // </if>
    cloudPrintInterface = new CloudPrintInterfaceStub();
    CloudPrintInterfaceImpl.setInstance(cloudPrintInterface);
    pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);
  });

  // Regression test for https://crbug.com/936029
  test(
      assert(print_preview_app_test.TestNames.PrintToGoogleDrive), async () => {
        await initialize();
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        const destinationSettings = sidebar.shadowRoot!.querySelector(
            'print-preview-destination-settings')!;
        // Set up the UI to have a cloud printer.
        destinationSettings.destination =
            getCloudDestination('FooCloud', 'FooName', 'foo@chromium.org');
        destinationSettings.destination.capabilities =
            getCddTemplate(destinationSettings.destination.id)!.capabilities;

        // Trigger print.
        sidebar.dispatchEvent(new CustomEvent(
            'print-requested', {composed: true, bubbles: true}));

        // Validate arguments to cloud print interface.
        const args = await cloudPrintInterface.whenCalled('submit');
        assertEquals('sample data', args.data);
        assertEquals('DocumentABC123', args.documentTitle);
        assertEquals('FooCloud', args.destination.id);
        assertEquals('1.0', JSON.parse(args.printTicket).version);
      });

  test(assert(print_preview_app_test.TestNames.PrintPresets), async () => {
    await initialize();
    assertEquals(1, page.settings.copies.value);
    assertFalse(page.settings.duplex.value);

    // Send preset values of duplex LONG_EDGE and 2 copies.
    const copies = 2;
    const duplex = DuplexMode.LONG_EDGE;
    webUIListenerCallback('print-preset-options', true, copies, duplex);
    assertEquals(copies, page.getSettingValue('copies') as number);
    assertTrue(page.getSettingValue('duplex') as boolean);
    assertFalse(page.getSetting('duplex').setFromUi);
    assertFalse(page.getSetting('copies').setFromUi);
  });

  test(
      assert(print_preview_app_test.TestNames.DestinationsManaged),
      async () => {
        initialSettings.destinationsManaged = true;
        await initialize();
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        assertTrue(sidebar.controlsManaged);
      });

  test(
      assert(print_preview_app_test.TestNames.HeaderFooterManaged),
      async () => {
        initialSettings.policies = {headerFooter: {allowedMode: true}};
        await initialize();
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        assertTrue(sidebar.controlsManaged);
      });

  test(
      assert(print_preview_app_test.TestNames.CssBackgroundManaged),
      async () => {
        initialSettings.policies = {cssBackground: {allowedMode: 1}};
        await initialize();
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        assertTrue(sidebar.controlsManaged);
      });

  test(assert(print_preview_app_test.TestNames.SheetsManaged), async () => {
    initialSettings.policies = {sheets: {value: 2}};
    await initialize();
    const sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    assertTrue(sidebar.controlsManaged);
  });
});
