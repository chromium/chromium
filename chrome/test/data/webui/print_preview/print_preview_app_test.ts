// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DuplexMode, NativeInitialSettings, NativeLayerImpl, PluginProxyImpl, PrintPreviewAppElement} from 'chrome://print/print_preview.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {TestPluginProxy} from './test_plugin_proxy.js';


const print_preview_app_test = {
  suiteName: 'PrintPreviewAppTest',
  TestNames: {
    PrintPresets: 'print presets',
    DestinationsManaged: 'destinations managed',
    HeaderFooterManaged: 'header footer managed',
    CssBackgroundManaged: 'css background managed',
    SheetsManaged: 'sheets managed',
  },
};

Object.assign(window, {print_preview_app_test: print_preview_app_test});

suite(print_preview_app_test.suiteName, function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

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
    // Stub out the native layer and the plugin.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);
  });

  test(print_preview_app_test.TestNames.PrintPresets, async () => {
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

  test(print_preview_app_test.TestNames.DestinationsManaged, async () => {
    initialSettings.destinationsManaged = true;
    await initialize();
    const sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    assertTrue(sidebar.controlsManaged);
  });

  test(print_preview_app_test.TestNames.HeaderFooterManaged, async () => {
    initialSettings.policies = {headerFooter: {allowedMode: true}};
    await initialize();
    const sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    assertTrue(sidebar.controlsManaged);
  });

  test(print_preview_app_test.TestNames.CssBackgroundManaged, async () => {
    initialSettings.policies = {cssBackground: {allowedMode: 1}};
    await initialize();
    const sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    assertTrue(sidebar.controlsManaged);
  });

  test(print_preview_app_test.TestNames.SheetsManaged, async () => {
    initialSettings.policies = {sheets: {value: 2}};
    await initialize();
    const sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    assertTrue(sidebar.controlsManaged);
  });
});
