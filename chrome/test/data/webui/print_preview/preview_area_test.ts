// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewPreviewAreaElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, Error, Margins, MarginsType, MeasurementSystem, MeasurementSystemUnitType, NativeLayerImpl, PluginProxyImpl, PreviewAreaState, Size, State} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('PreviewAreaTest', function() {
  let previewArea: PrintPreviewPreviewAreaElement;
  let nativeLayer: NativeLayerStub;
  let pluginProxy: TestPluginProxy;

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayer.setPageCount(3);
    pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);
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
  test('StateChanges', async function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    assertEquals(PreviewAreaState.LOADING, previewArea.previewState);
    assertFalse(
        previewArea.shadowRoot.querySelector('.preview-area-overlay-layer')!
            .classList.contains('invisible'));
    const message =
        previewArea.shadowRoot.querySelector('.preview-area-message')!
            .querySelector('span')!;
    assertEquals('Loading preview', message.textContent!.trim());

    previewArea.startPreview(false);

    await whenPreviewStarted;
    await microtasksFinished();

    assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
    assertEquals(3, pluginProxy.getCallCount('loadPreviewPage'));
    assertTrue(
        previewArea.shadowRoot.querySelector('.preview-area-overlay-layer')!
            .classList.contains('invisible'));

    // If destination capabilities fetch fails, the invalid printer error
    // will be set by the destination settings.
    previewArea.destination = new Destination(
        'InvalidDevice', DestinationOrigin.LOCAL, 'InvalidName');
    previewArea.state = State.ERROR;
    previewArea.error = Error.INVALID_PRINTER;
    await microtasksFinished();

    assertEquals(PreviewAreaState.ERROR, previewArea.previewState);
    assertFalse(
        previewArea.shadowRoot.querySelector('.preview-area-overlay-layer')!
            .classList.contains('invisible'));
    assertEquals(
        'The selected printer is not available or not installed ' +
            'correctly.  Check your printer or try selecting another ' +
            'printer.',
        message.textContent!.trim());
  });

  /** Validate preview area sets tabindex correctly based on viewport size. */
  test('ViewportSizeChanges', async function() {
    // Simulate starting the preview.
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    previewArea.startPreview(false);

    await whenPreviewStarted;
    await microtasksFinished();

    assertEquals(PreviewAreaState.DISPLAY_PREVIEW, previewArea.previewState);
    assertTrue(
        previewArea.shadowRoot.querySelector('.preview-area-overlay-layer')!
            .classList.contains('invisible'));
    const plugin =
        previewArea.shadowRoot.querySelector('.preview-area-plugin')!;
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

  test('PointerEvents', async function() {
    const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
    previewArea.state = State.READY;
    previewArea.startPreview(false);
    await whenPreviewStarted;
    await microtasksFinished();

    const marginControls =
        previewArea.$.marginControlContainer.shadowRoot.querySelectorAll(
            'print-preview-margin-control');
    assertEquals(4, marginControls.length);

    function assertVisible(visible: boolean) {
      for (const control of marginControls) {
        assertEquals(visible, !control.invisible);
      }
    }

    assertVisible(false);

    previewArea.setSetting('margins', MarginsType.CUSTOM);
    await microtasksFinished();
    assertVisible(true);

    previewArea.dispatchEvent(new PointerEvent('pointerout', {pointerId: 1}));
    await microtasksFinished();
    assertVisible(false);

    previewArea.dispatchEvent(new PointerEvent('pointerover', {pointerId: 1}));
    await microtasksFinished();
    assertVisible(true);
  });
});
