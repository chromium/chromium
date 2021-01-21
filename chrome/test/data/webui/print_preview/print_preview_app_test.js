// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterface, CloudPrintInterfaceImpl, Destination, DuplexMode, NativeLayer, NativeLayerImpl, PluginProxyImpl} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {getCddTemplate, getGoogleDriveDestination} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {TestPluginProxy} from 'chrome://test/print_preview/test_plugin_proxy.js';

// <if expr="chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

window.print_preview_app_test = {};
print_preview_app_test.suiteName = 'PrintPreviewAppTest';
/** @enum {string} */
print_preview_app_test.TestNames = {
  PrintToGoogleDrive: 'print to google drive',
  PrintPresets: 'print presets',
  DestinationsManaged: 'destinations managed',
  HeaderFooterManaged: 'header footer managed',
  CssBackgroundManaged: 'css background managed',
  SheetsManaged: 'sheets managed'
};

suite(print_preview_app_test.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {?CloudPrintInterface} */
  let cloudPrintInterface = null;

  /** @type {?TestPluginProxy} */
  let pluginProxy = null;

  /** @type {!NativeInitialSettings} */
  const initialSettings = {
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
    userAccounts: ['foo@chromium.org'],
  };

  /**
   * Set the native layer initial settings and attach the print preview app to
   * the DOM.
   * @return {!Promise} Returns a promise that resolves when the 'getPreview'
   *     method is called by the preview area contained inside the app.
   */
  const initialize = () => {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    return nativeLayer.whenCalled('getPreview');
  };

  /** @override */
  setup(function() {
    // Stub out the native layer, the cloud print interface, and the plugin.
    document.body.innerHTML = '';
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    // <if expr="chromeos">
    setNativeLayerCrosInstance();
    // </if>
    cloudPrintInterface = new CloudPrintInterfaceStub();
    CloudPrintInterfaceImpl.instance_ = cloudPrintInterface;
    pluginProxy = new TestPluginProxy();
    PluginProxyImpl.instance_ = pluginProxy;
  });

  // Regression test for https://crbug.com/936029
  test(
      assert(print_preview_app_test.TestNames.PrintToGoogleDrive), async () => {
        await initialize();
        // Set up the UI to have Google Drive as the printer.
        page.destination_ = getGoogleDriveDestination('foo@chromium.org');
        page.destination_.capabilities = getCddTemplate(page.destination_.id);

        // Trigger print.
        const sidebar = page.$$('print-preview-sidebar');
        sidebar.dispatchEvent(new CustomEvent(
            'print-requested', {composed: true, bubbles: true}));

        // Validate arguments to cloud print interface.
        const args = await cloudPrintInterface.whenCalled('submit');
        assertEquals('sample data', args.data);
        assertEquals('DocumentABC123', args.documentTitle);
        assertEquals(Destination.GooglePromotedId.DOCS, args.destination.id);
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
    assertEquals(copies, page.getSettingValue('copies'));
    assertTrue(page.getSettingValue('duplex'));
    assertFalse(page.getSetting('duplex').setFromUi);
    assertFalse(page.getSetting('copies').setFromUi);
  });

  test(
      assert(print_preview_app_test.TestNames.DestinationsManaged),
      async () => {
        initialSettings.destinationsManaged = true;
        await initialize();
        const sidebar = page.$$('print-preview-sidebar');
        assertTrue(sidebar.controlsManaged);
      });

  test(
      assert(print_preview_app_test.TestNames.HeaderFooterManaged),
      async () => {
        initialSettings.policies = {headerFooter: {allowedMode: true}};
        await initialize();
        const sidebar = page.$$('print-preview-sidebar');
        assertTrue(sidebar.controlsManaged);
      });

  test(
      assert(print_preview_app_test.TestNames.CssBackgroundManaged),
      async () => {
        initialSettings.policies = {cssBackground: {allowedMode: 1}};
        await initialize();
        const sidebar = page.$$('print-preview-sidebar');
        assertTrue(sidebar.controlsManaged);
      });

  test(assert(print_preview_app_test.TestNames.SheetsManaged), async () => {
    initialSettings.policies = {sheets: {value: 2}};
    await initialize();
    const sidebar = page.$$('print-preview-sidebar');
    assertTrue(sidebar.controlsManaged);
  });
});
