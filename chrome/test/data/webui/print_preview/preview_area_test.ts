// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationOrigin, Error, Margins, MeasurementSystem, MeasurementSystemUnitType, NativeLayerImpl, PluginProxyImpl, PreviewAreaState, PrintPreviewPreviewAreaElement, Size, State} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

const preview_area_test = {
  suiteName: 'PreviewAreaTest',
  TestNames: {
    StateChanges: 'state changes',
    ViewportSizeChanges: 'viewport size changes',
  },
};

Object.assign(window, {preview_area_test: preview_area_test});

suite(preview_area_test.suiteName, function() {
  let previewArea: PrintPreviewPreviewAreaElement;

  let nativeLayer: NativeLayerStub;

  let pluginProxy: TestPluginProxy;

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayer.setPageCount(3);
    pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

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
  });

  /** Validate some preview area state transitions work as expected. */
  test(preview_area_test.TestNames.StateChanges, function() {
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

  /** Validate preview area sets tabindex correctly based on viewport size. */
  test(preview_area_test.TestNames.ViewportSizeChanges, function() {
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
