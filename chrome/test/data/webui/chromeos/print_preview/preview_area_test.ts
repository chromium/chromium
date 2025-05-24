// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewPreviewAreaElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, Error, Margins, MeasurementSystem, MeasurementSystemUnitType, NativeLayerImpl, PluginProxyImpl, PreviewAreaState, Size, State} from 'chrome://print/print_preview.js';
// <if expr="is_chromeos">
// clang-format off
import {PrinterSetupInfoMessageType, PrintPreviewPrinterSetupInfoCrosElement} from 'chrome://print/print_preview.js';
// clang-format on
// </if>
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
// <if expr="is_chromeos">
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import type {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('PreviewAreaTest', function() {
  let previewArea: PrintPreviewPreviewAreaElement;

  let nativeLayer: NativeLayerStub;

  let pluginProxy: TestPluginProxy;

  // <if expr="is_chromeos">
  let nativeLayerCros: NativeLayerCrosStub;
  // </if>

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayer.setPageCount(3);
    pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);
    // <if expr="is_chromeos">
    nativeLayerCros = setNativeLayerCrosInstance();
    // </if>

    setupPreviewElement();
  });

  /** Configures preview element for tests. */
  function setupPreviewElement(): void {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.setSetting('pages', [1, 2, 3]);
    previewArea = document.createElement('print-preview-preview-area');
    document.body.appendChild(previewArea);
    previewArea.settings = model.settings;
    fakeDataBind(model, previewArea, 'settings');
    previewArea.destination =
        new Destination('FooDevice', DestinationOrigin.LOCAL, 'FooName');
    previewArea.destination.capabilities =
        getCddTemplate('FooDevice').capabilities;
    previewArea.error = Error.NONE;
    previewArea.state = State.NOT_READY;
    previewArea.documentModifiable = true;
    previewArea.measurementSystem =
        new MeasurementSystem(',', '.', MeasurementSystemUnitType.IMPERIAL);

    previewArea.pageSize = new Size(612, 794);
    previewArea.margins = new Margins(10, 10, 10, 10);
  }

  /** Validate some preview area state transitions work as expected. */
  test('StateChanges', function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    assertEquals(PreviewAreaState.LOADING, previewArea.previewState);
    assertFalse(
        previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
            .classList.contains('invisible'));
    const message =
        previewArea.shadowRoot!.querySelector('.preview-area-message')!
            .querySelector('span')!;
    assertEquals('Loading preview', message.textContent!.trim());

    previewArea.startPreview(false);

    return whenPreviewStarted.then(() => {
      assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
      assertEquals(3, pluginProxy.getCallCount('loadPreviewPage'));
      assertTrue(
          previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
              .classList.contains('invisible'));

      // If destination capabilities fetch fails, the invalid printer error
      // will be set by the destination settings.
      previewArea.destination = new Destination(
          'InvalidDevice', DestinationOrigin.LOCAL, 'InvalidName');
      previewArea.state = State.ERROR;
      previewArea.error = Error.INVALID_PRINTER;
      assertEquals(PreviewAreaState.ERROR, previewArea.previewState);
      assertFalse(
          previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
              .classList.contains('invisible'));
      assertEquals(
          'The selected printer is not available or not installed ' +
              'correctly.  Check your printer or try selecting another ' +
              'printer.',
          message.textContent!.trim());
    });
  });

  // <if expr="is_chromeos">
  /**
   * Validate some preview area state transitions work as expected on CrOS with
   * Printer Setup Assistance flag enabled.
   */
  test('StateChangesPrinterSetupCros', function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    assertEquals(PreviewAreaState.LOADING, previewArea.previewState);
    assertFalse(
        previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
            .classList.contains('invisible'));
    const message =
        previewArea.shadowRoot!.querySelector('.preview-area-message')!
            .querySelector('span')!;
    assertEquals('Loading preview', message.textContent!.trim());

    previewArea.startPreview(false);

    return whenPreviewStarted.then(() => {
      assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
      assertEquals(3, pluginProxy.getCallCount('loadPreviewPage'));
      assertTrue(
          previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
              .classList.contains('invisible'));

      // If destination capabilities fetch fails, the invalid printer error
      // will be set by the destination settings.
      previewArea.destination = new Destination(
          'InvalidDevice', DestinationOrigin.LOCAL, 'InvalidName');
      previewArea.state = State.ERROR;
      previewArea.error = Error.INVALID_PRINTER;
      assertEquals(PreviewAreaState.ERROR, previewArea.previewState);
      assertFalse(
          previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
              .classList.contains('invisible'));
      assertFalse(isChildVisible(previewArea, '.preview-area-message > span'));
      assertTrue(isChildVisible(
          previewArea, PrintPreviewPrinterSetupInfoCrosElement.is));
      assertEquals(
          PrinterSetupInfoMessageType.PRINTER_OFFLINE,
          previewArea.shadowRoot!
              .querySelector(
                  PrintPreviewPrinterSetupInfoCrosElement.is)!.messageType);
    });
  });

  // Verify correct metric is triggered when launch printer settings button
  // is pressed from preview-area error state.
  test('ManagePrinterMetricsCros', function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    previewArea.startPreview(false);

    // Metrics functions to verify.
    const managePrintersFunction = 'managePrinters';
    const recordMetricFunction = 'recordInHistogram';

    return whenPreviewStarted
        .then(async () => {
          // If destination capabilities fetch fails, the invalid printer error
          // will be set by the destination settings.
          previewArea.destination = new Destination(
              'InvalidDevice', DestinationOrigin.LOCAL, 'InvalidName');
          previewArea.state = State.ERROR;
          previewArea.error = Error.INVALID_PRINTER;
          assertEquals(0, nativeLayer.getCallCount(managePrintersFunction));
          assertEquals(0, nativeLayer.getCallCount(recordMetricFunction));

          return nativeLayerCros.whenCalled('getShowManagePrinters');
        })
        .then(() => {
          // Click button to launch settings.
          const setupInfoElement = previewArea.shadowRoot!.querySelector(
              PrintPreviewPrinterSetupInfoCrosElement.is)!;
          const managePrintersButton =
              setupInfoElement.shadowRoot!.querySelector('cr-button')!;
          managePrintersButton.click();

          // Verify manage printers button clicked and triggers recording
          // histogram.
          assertEquals(1, nativeLayer.getCallCount(managePrintersFunction));
          assertEquals(1, nativeLayer.getCallCount(recordMetricFunction));
          const call = nativeLayer.getArgs(recordMetricFunction)[0];
          assertEquals('PrintPreview.PrinterSettingsLaunchSource', call[0]);
          // Call should use bucket `PREVIEW_AREA_CONNECTION_ERROR`.
          assertEquals(0, call[1]);
        });
  });
  // </if>

  /** Validate preview area sets tabindex correctly based on viewport size. */
  test('ViewportSizeChanges', function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    previewArea.startPreview(false);

    return whenPreviewStarted.then(() => {
      assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
      assertTrue(
          previewArea.shadowRoot!.querySelector('.preview-area-overlay-layer')!
              .classList.contains('invisible'));
      const plugin =
          previewArea.shadowRoot!.querySelector('.preview-area-plugin')!;
      assertEquals(null, plugin.getAttribute('tabindex'));

      // This can be triggered at any time by a resizing of the viewport or
      // change to the PDF viewer zoom.
      // Plugin is too narrow to show zoom toolbar.
      pluginProxy.triggerVisualStateChange(0, 0, 150, 150, 500);
      assertEquals('-1', plugin.getAttribute('tabindex'));
      // Plugin is large enough for zoom toolbar.
      pluginProxy.triggerVisualStateChange(0, 0, 250, 400, 500);
      assertEquals('0', plugin.getAttribute('tabindex'));
      // Plugin is too short for zoom toolbar.
      pluginProxy.triggerVisualStateChange(0, 0, 250, 400, 100);
      assertEquals('-1', plugin.getAttribute('tabindex'));
      // Plugin is large enough for zoom toolbar.
      pluginProxy.triggerVisualStateChange(0, 0, 500, 800, 1000);
      assertEquals('0', plugin.getAttribute('tabindex'));
    });
  });
});
