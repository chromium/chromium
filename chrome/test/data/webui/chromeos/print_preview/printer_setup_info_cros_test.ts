// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayerImpl, PrinterSetupInfoInitiator, PrinterSetupInfoMessageType, PrintPreviewPrinterSetupInfoCrosElement} from 'chrome://print/print_preview.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import type {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';

suite('PrinterSetupInfoTest', function() {
  let parentDiv: HTMLElement;
  let setupInfoElement: PrintPreviewPrinterSetupInfoCrosElement;
  let nativeLayer: NativeLayerStub;
  let nativeLayerCros: NativeLayerCrosStub;

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayerCros = setNativeLayerCrosInstance();
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /** @return {T} Element from shadow root */
  function getShadowElement<T extends HTMLElement>(
      parentElement: PolymerElement, selector: string): T {
    assertTrue(!!parentElement);
    const element = parentElement.shadowRoot!.querySelector<T>(selector);
    assertTrue(!!element);
    return element;
  }

  /** Appends `PrintPreviewPrinterSetupInfoCrosElement` to document body. */
  async function setupElement(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    parentDiv = document.createElement('div');
    setupInfoElement =
        document.createElement(PrintPreviewPrinterSetupInfoCrosElement.is);
    parentDiv.appendChild(setupInfoElement);
    document.body.appendChild(parentDiv);
    flush();
    await nativeLayerCros.whenCalled('getShowManagePrinters');
  }


  /**
   * Checks that `recordInHistogram` is called with expected bucket.
   */
  function verifyRecordInHistogramCall(
      callIndex: number, expectedBucket: number): void {
    const calls = nativeLayer.getArgs('recordInHistogram');
    assertTrue(!!calls && calls.length > 0);
    assertTrue(callIndex < calls.length);
    const [histogramName, bucket] = calls[callIndex];
    assertEquals('PrintPreview.PrinterSettingsLaunchSource', histogramName);
    assertEquals(expectedBucket, bucket);
  }

  /** Verifies element can be added to UI and display. */
  test('ElementDisplays', async function() {
    await setupElement();

    assertTrue(!!setupInfoElement);
    assertTrue(isChildVisible(setupInfoElement, 'cr-button'));
    assertTrue(isChildVisible(setupInfoElement, '.message-heading'));
    assertTrue(isChildVisible(setupInfoElement, '.message-detail'));
    assertTrue(isChildVisible(setupInfoElement, 'cr-icon'));
  });

  /** Verifies button text is localized. */
  test(
      'ButtonLocalized', async function() {
        await setupElement();

        const managePrintersLabelKey = 'managePrintersLabel';
        assertTrue(setupInfoElement.i18nExists(managePrintersLabelKey));
        const managePrintersButton =
            getShadowElement<CrButtonElement>(setupInfoElement, 'cr-button');
        assertEquals(
            setupInfoElement.i18n(managePrintersLabelKey),
            managePrintersButton.textContent!.trim());
      });

  /**
   * Verifies manage printers button invokes launch settings from native layer.
   */
  test('ManagePrintersButton', async function() {
    await setupElement();
    assertEquals(0, nativeLayer.getCallCount('managePrinters'));

    // Click button. Set the initiator so the correct metric is recorded.
    setupInfoElement.setInitiatorForTesting(
        PrinterSetupInfoInitiator.PREVIEW_AREA, /*startResizeObserver=*/ false);
    const managePrinters =
        getShadowElement<CrButtonElement>(setupInfoElement, 'cr-button');
    managePrinters.click();

    assertEquals(1, nativeLayer.getCallCount('managePrinters'));
  });

  /** Verify correct localized message displayed for message type. */
  test(
      'MessageMatchesMessageType', async function() {
        await setupElement();

        // Default message type configured to be "no-printers".
        assertEquals(
            PrinterSetupInfoMessageType.NO_PRINTERS,
            setupInfoElement.messageType);

        // Localized keys exist for "no-printers" message type.
        const noPrintersMessageHeadingLabelKey =
            'printerSetupInfoMessageHeadingNoPrintersText';
        const noPrintersMessageDetailLabelKey =
            'printerSetupInfoMessageDetailNoPrintersText';
        assertTrue(
            setupInfoElement.i18nExists(noPrintersMessageHeadingLabelKey));
        assertTrue(
            setupInfoElement.i18nExists(noPrintersMessageDetailLabelKey));

        // Expected localized message displays correctly for "no-printers"
        // message type.
        const messageHeading =
            setupInfoElement.shadowRoot!.querySelector<HTMLElement>(
                '.message-heading')!;
        const messageDetail =
            setupInfoElement.shadowRoot!.querySelector<HTMLElement>(
                '.message-detail')!;
        assertEquals(
            setupInfoElement.i18n(noPrintersMessageHeadingLabelKey),
            messageHeading.textContent!.trim());
        assertEquals(
            setupInfoElement.i18n(noPrintersMessageDetailLabelKey),
            messageDetail.textContent!.trim());

        // Change message type to 'printer-offline'.
        setupInfoElement.messageType =
            PrinterSetupInfoMessageType.PRINTER_OFFLINE;
        flush();

        // Localized keys exist for "printer-offline" message type.
        const printerOfflineMessageHeadingLabelKey =
            'printerSetupInfoMessageHeadingPrinterOfflineText';
        const printerOfflineMessageDetailLabelKey =
            'printerSetupInfoMessageDetailPrinterOfflineText';
        assertTrue(
            setupInfoElement.i18nExists(printerOfflineMessageHeadingLabelKey));
        assertTrue(
            setupInfoElement.i18nExists(printerOfflineMessageDetailLabelKey));

        // Expected localized message displays for "printer-offline" message
        // type.
        assertEquals(
            setupInfoElement.i18n(printerOfflineMessageHeadingLabelKey),
            messageHeading.textContent!.trim());
        assertEquals(
            setupInfoElement.i18n(printerOfflineMessageDetailLabelKey),
            messageDetail.textContent!.trim());
      });

  /**
   * Verifies manage printers button invokes launch settings metric.
   */
  test(
      'ManagePrintersButtonMetrics', async function() {
        await setupElement();
        const recordMetricsFunction = 'recordInHistogram';
        assertEquals(0, nativeLayer.getCallCount(recordMetricsFunction));

        // Set metrics source to destination-dialog-cros and click.
        setupInfoElement.setInitiatorForTesting(
            PrinterSetupInfoInitiator.DESTINATION_DIALOG_CROS,
            /*startResizeObserver=*/ false);
        const managePrinters =
            getShadowElement<CrButtonElement>(setupInfoElement, 'cr-button');
        managePrinters.click();

        // Call should use bucket `DESTINATION_DIALOG_CROS_NO_PRINTERS`.
        verifyRecordInHistogramCall(/*callIndex=*/ 0, /*expectedBucket=*/ 1);

        // Set metrics source to destination-dialog-cros and click.
        setupInfoElement.setInitiatorForTesting(
            PrinterSetupInfoInitiator.PREVIEW_AREA,
            /*startResizeObserver=*/ false);
        managePrinters.click();

        // Call should use bucket `PREVIEW_AREA_CONNECTION_ERROR`.
        verifyRecordInHistogramCall(/*callIndex=*/ 1, /*expectedBucket=*/ 0);
      });

  /**
   * Verifies manage printers button hidden when getShowManagePrinters returns
   * false.
   */
  test('DoNotShowManagePrinters', async function() {
    nativeLayerCros.setShowManagePrinters(false);
    await setupElement();

    assertTrue(!!setupInfoElement);
    assertTrue(isChildVisible(setupInfoElement, '.message-heading'));
    assertTrue(isChildVisible(setupInfoElement, '.message-detail'));
    assertFalse(isChildVisible(setupInfoElement, 'cr-button'));
  });

  /**
   * Verifies the illustration is hidden when the Print Preview parent element
   * becomes too small then reappears when the element is expanded.
   */
  test('HideIllustrationForSmallWindow', async function() {
    await setupElement();
    parentDiv.style.height = '1000px';
    parentDiv.style.width = '1000px';
    parentDiv.classList.add('preview-area-message');
    setupInfoElement.setInitiatorForTesting(
        PrinterSetupInfoInitiator.PREVIEW_AREA, /*startResizeObserver=*/ true);

    // Verify the illustration is showing initially.
    assertTrue(isChildVisible(setupInfoElement, 'cr-icon'));

    // Reducing the width should hide the illustration.
    parentDiv.style.width = '249px';
    await waitAfterNextRender(parentDiv);
    await waitAfterNextRender(setupInfoElement);
    assertFalse(isChildVisible(setupInfoElement, 'cr-icon'));

    // Expanding the width should show the illustration.
    parentDiv.style.width = '1000px';
    await waitAfterNextRender(parentDiv);
    await waitAfterNextRender(setupInfoElement);
    assertTrue(isChildVisible(setupInfoElement, 'cr-icon'));

    // Reducing the height should hide the illustration.
    parentDiv.style.height = '399px';
    await waitAfterNextRender(parentDiv);
    await waitAfterNextRender(setupInfoElement);
    assertFalse(isChildVisible(setupInfoElement, 'cr-icon'));

    // Expanding the height should show the illustration.
    parentDiv.style.height = '1000px';
    await waitAfterNextRender(parentDiv);
    await waitAfterNextRender(setupInfoElement);
    assertTrue(isChildVisible(setupInfoElement, 'cr-icon'));
  });
});
