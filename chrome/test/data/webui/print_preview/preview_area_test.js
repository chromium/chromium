// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, Error, Margins, MeasurementSystem, MeasurementSystemUnitType, NativeLayer, PluginProxy, PreviewAreaState, Size, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplate} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {fakeDataBind} from 'chrome://test/test_util.m.js';

window.preview_area_test = {};
preview_area_test.suiteName = 'PreviewAreaTest';
/** @enum {string} */
preview_area_test.TestNames = {
  StateChanges: 'state changes',
  ViewportSizeChanges: 'viewport size changes',
};

suite(preview_area_test.suiteName, function() {
  /** @type {?PrintPreviewPreviewAreaElement} */
  let previewArea = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  let pluginProxy = null;

  /** @override */
  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);
    nativeLayer.setPageCount(3);
    pluginProxy = new PDFPluginStub();
    PluginProxy.setInstance(pluginProxy);

    PolymerTest.clearBody();
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.setSetting('pages', [1, 2, 3]);
    previewArea = document.createElement('print-preview-preview-area');
    document.body.appendChild(previewArea);
    previewArea.settings = model.settings;
    fakeDataBind(model, previewArea, 'settings');
    previewArea.destination = new Destination(
        'FooDevice', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'FooName',
        DestinationConnectionStatus.ONLINE);
    previewArea.destination.capabiliites = getCddTemplate('FooDevice');
    previewArea.error = Error.NONE;
    previewArea.state = State.NOT_READY;
    previewArea.documentModifiable = true;
    previewArea.measurementSystem =
        new MeasurementSystem(',', '.', MeasurementSystemUnitType.IMPERIAL);

    previewArea.pageSize = new Size(612, 794);
    previewArea.margins = new Margins(10, 10, 10, 10);
  });

  /** Validate some preview area state transitions work as expected. */
  test(assert(preview_area_test.TestNames.StateChanges), function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    assertEquals(PreviewAreaState.LOADING, previewArea.previewState);
    assertFalse(previewArea.$$('.preview-area-overlay-layer')
                    .classList.contains('invisible'));
    const message =
        previewArea.$$('.preview-area-message').querySelector('span');
    assertEquals('Loading preview', message.textContent.trim());

    previewArea.startPreview();

    return whenPreviewStarted.then(() => {
      assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
      assertEquals(3, pluginProxy.getCallCount('loadPreviewPage'));
      assertTrue(previewArea.$$('.preview-area-overlay-layer')
                     .classList.contains('invisible'));

      // If destination capabilities fetch fails, the invalid printer error
      // will be set by the destination settings.
      previewArea.destination = new Destination(
          'InvalidDevice', DestinationType.LOCAL, DestinationOrigin.LOCAL,
          'InvalidName', DestinationConnectionStatus.ONLINE);
      previewArea.state = State.ERROR;
      previewArea.error = Error.INVALID_PRINTER;
      assertEquals(PreviewAreaState.ERROR, previewArea.previewState);
      assertFalse(previewArea.$$('.preview-area-overlay-layer')
                      .classList.contains('invisible'));
      assertEquals(
          'The selected printer is not available or not installed ' +
              'correctly.  Check your printer or try selecting another ' +
              'printer.',
          message.textContent.trim());
    });
  });

  /** Validate preview area sets tabindex correctly based on viewport size. */
  test(assert(preview_area_test.TestNames.ViewportSizeChanges), function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    previewArea.startPreview();

    return whenPreviewStarted.then(() => {
      assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
      assertTrue(previewArea.$$('.preview-area-overlay-layer')
                     .classList.contains('invisible'));
      const plugin = previewArea.$$('.preview-area-plugin');
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
