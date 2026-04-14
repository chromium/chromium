// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NativeInitialSettings, PreviewTicket, PrintPreviewAppElement, Settings} from 'chrome://print/print_preview.js';
import {ColorMode, CustomMarginsOrientation, Destination, DestinationOrigin, Margins, MarginsType, NativeLayerImpl, PluginProxyImpl, ScalingType} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate, getDefaultInitialSettings} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

interface ValidateScalingChangeParams {
  printTicket: string;
  scalingTypeKey: keyof Settings;
  expectedTicketId: number;
  expectedTicketScaleFactor: number;
  expectedScalingValue: string;
  expectedScalingType: ScalingType;
}

suite('PreviewGenerationTest', function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

  const initialSettings: NativeInitialSettings = getDefaultInitialSettings();

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * Initializes the UI with a default local destination and a 3 page document
   * length.
   * @return Promise that resolves when initialization is done,
   *     destination is set, and initial preview request is complete.
   */
  async function initialize(): Promise<{printTicket: string}> {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    nativeLayer.setPageCount(3);
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    const documentInfo = page.$.documentInfo;
    documentInfo.documentSettings.pageCount = 3;
    documentInfo.margins = new Margins(10, 10, 10, 10);

    await Promise.all([
      nativeLayer.whenCalled('getInitialSettings'),
      nativeLayer.whenCalled('getPrinterCapabilities'),
    ]);
    return nativeLayer.whenCalled('getPreview');
  }

  type SimpleSettingType = boolean|string|number;

  /**
   * @param settingName The name of the setting to test.
   * @param initialSettingValue The default setting value.
   * @param updatedSettingValue The setting value to update
   *     to.
   * @param ticketKey The field in the print ticket that corresponds
   *     to the setting.
   * @param initialTicketValue The ticket value
   *     corresponding to the default setting value.
   * @param updatedTicketValue The ticket value
   *     corresponding to the updated setting value.
   * @return Promise that resolves when the setting has been
   *     changed, the preview has been regenerated, and the print ticket and
   *     UI state have been verified.
   */
  async function testSimpleSetting(
      settingName: keyof Settings, initialSettingValue: SimpleSettingType,
      updatedSettingValue: SimpleSettingType, ticketKey: string,
      initialTicketValue: SimpleSettingType,
      updatedTicketValue: SimpleSettingType): Promise<void> {
    let args = await initialize();
    const originalTicket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(0, originalTicket.requestID);
    const indexableOriginalTicket = originalTicket as {[key: string]: any};
    assertEquals(initialTicketValue, indexableOriginalTicket[ticketKey]);
    nativeLayer.resetResolver('getPreview');
    assertEquals(initialSettingValue, page.getSettingValue(settingName));
    page.setSetting(settingName, updatedSettingValue);
    args = await nativeLayer.whenCalled('getPreview');
    assertEquals(updatedSettingValue, page.getSettingValue(settingName));
    const ticket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(
        updatedTicketValue, (ticket as {[key: string]: any})[ticketKey]);
    assertEquals(1, ticket.requestID);
  }

  function validateScalingChange(input: ValidateScalingChangeParams) {
    const ticket: PreviewTicket = JSON.parse(input.printTicket);
    assertEquals(input.expectedTicketId, ticket.requestID);
    assertEquals(input.expectedTicketScaleFactor, ticket.scaleFactor);
    assertEquals(input.expectedScalingValue, page.getSettingValue('scaling'));
    assertEquals(
        input.expectedScalingType, page.getSettingValue(input.scalingTypeKey));
  }

  /** Validate changing the color updates the preview. */
  test('Color', function() {
    return testSimpleSetting(
        'color', true, false, 'color', ColorMode.COLOR, ColorMode.GRAY);
  });

  /** Validate changing the background setting updates the preview. */
  test('CssBackground', function() {
    return testSimpleSetting(
        'cssBackground', false, true, 'shouldPrintBackgrounds', false, true);
  });

  /** Validate changing the header/footer setting updates the preview. */
  test('HeaderFooter', function() {
    return testSimpleSetting(
        'headerFooter', true, false, 'headerFooterEnabled', true, false);
  });

  /** Validate changing the orientation updates the preview. */
  test('Layout', function() {
    return testSimpleSetting('layout', false, true, 'landscape', false, true);
  });

  /** Validate changing the margins updates the preview. */
  test('Margins', function() {
    return testSimpleSetting(
        'margins', MarginsType.DEFAULT, MarginsType.MINIMUM, 'marginsType',
        MarginsType.DEFAULT, MarginsType.MINIMUM);
  });

  /**
   * Validate changing the custom margins updates the preview, only after all
   * values have been set.
   */
  test('CustomMargins', async () => {
    const args = await initialize();
    const originalTicket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(MarginsType.DEFAULT, originalTicket.marginsType);
    // Custom margins should not be set in the ticket.
    assertEquals(undefined, originalTicket.marginsCustom);
    assertEquals(0, originalTicket.requestID);

    // This should do nothing.
    page.setSetting('margins', MarginsType.CUSTOM);
    await microtasksFinished();
    // Sets only 1 side, not valid.
    page.setSetting('customMargins', {marginTop: 25});
    await microtasksFinished();
    // 2 sides, still not valid.
    page.setSetting('customMargins', {marginTop: 25, marginRight: 40});
    await microtasksFinished();
    // This should trigger a preview.
    nativeLayer.resetResolver('getPreview');
    page.setSetting('customMargins', {
      marginTop: 25,
      marginRight: 40,
      marginBottom: 20,
      marginLeft: 50,
    });
    let previewArgs = await nativeLayer.whenCalled('getPreview');
    const ticket: PreviewTicket = JSON.parse(previewArgs.printTicket);
    assertEquals(MarginsType.CUSTOM, ticket.marginsType);
    assertEquals(25, ticket.marginsCustom!.marginTop);
    assertEquals(40, ticket.marginsCustom!.marginRight);
    assertEquals(20, ticket.marginsCustom!.marginBottom);
    assertEquals(50, ticket.marginsCustom!.marginLeft);
    assertEquals(1, ticket.requestID);
    page.setSetting('margins', MarginsType.DEFAULT);
    // Set setting to something invalid and then set margins to CUSTOM.
    page.setSetting('customMargins', {marginTop: 25, marginRight: 40});
    await microtasksFinished();
    page.setSetting('margins', MarginsType.CUSTOM);
    await microtasksFinished();
    nativeLayer.resetResolver('getPreview');
    page.setSetting('customMargins', {
      marginTop: 25,
      marginRight: 40,
      marginBottom: 20,
      marginLeft: 50,
    });
    previewArgs = await nativeLayer.whenCalled('getPreview');
    const ticketUpdated: PreviewTicket = JSON.parse(previewArgs.printTicket);
    assertEquals(MarginsType.CUSTOM, ticketUpdated.marginsType);
    assertEquals(25, ticketUpdated.marginsCustom!.marginTop);
    assertEquals(40, ticketUpdated.marginsCustom!.marginRight);
    assertEquals(20, ticketUpdated.marginsCustom!.marginBottom);
    assertEquals(50, ticketUpdated.marginsCustom!.marginLeft);
    // Request 3. Changing to default margins should have triggered a
    // preview, and the final setting of valid custom margins should
    // have triggered another one.
    assertEquals(3, ticketUpdated.requestID);
  });

  /**
   * Validate changing the pages per sheet updates the preview, and resets
   * margins to MarginsType.DEFAULT.
   */
  test('ChangeMarginsByPagesPerSheet', async () => {
    let args = await initialize();
    const originalTicket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(0, originalTicket.requestID);
    assertEquals(MarginsType.DEFAULT, originalTicket['marginsType']);
    assertEquals(MarginsType.DEFAULT, page.getSettingValue('margins'));
    assertEquals(1, page.getSettingValue('pagesPerSheet'));
    assertEquals(1, originalTicket['pagesPerSheet']);
    nativeLayer.resetResolver('getPreview');
    page.setSetting('margins', MarginsType.MINIMUM);
    args = await nativeLayer.whenCalled('getPreview');
    assertEquals(MarginsType.MINIMUM, page.getSettingValue('margins'));
    const ticket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(MarginsType.MINIMUM, ticket['marginsType']);
    nativeLayer.resetResolver('getPreview');
    assertEquals(1, ticket.requestID);
    page.setSetting('pagesPerSheet', 4);
    args = await nativeLayer.whenCalled('getPreview');
    assertEquals(MarginsType.DEFAULT, page.getSettingValue('margins'));
    assertEquals(4, page.getSettingValue('pagesPerSheet'));
    const ticketUpdated: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(MarginsType.DEFAULT, ticketUpdated['marginsType']);
    assertEquals(4, ticketUpdated['pagesPerSheet']);
    assertEquals(2, ticketUpdated.requestID);
  });

  /** Validate changing the paper size updates the preview. */
  test('MediaSize', async () => {
    const mediaSizeCapability =
        getCddTemplate('FooDevice').capabilities!.printer.media_size!;
    const letterOption = mediaSizeCapability.option[0]!;
    const squareOption = mediaSizeCapability.option[1]!;
    let args = await initialize();
    const originalTicket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(
        letterOption.width_microns, originalTicket.mediaSize.width_microns);
    assertEquals(
        letterOption.height_microns, originalTicket.mediaSize.height_microns);
    assertEquals(0, originalTicket.requestID);
    nativeLayer.resetResolver('getPreview');
    const mediaSizeSetting = page.getSettingValue('mediaSize');
    assertEquals(letterOption.width_microns, mediaSizeSetting.width_microns);
    assertEquals(letterOption.height_microns, mediaSizeSetting.height_microns);
    page.setSetting('mediaSize', squareOption);
    args = await nativeLayer.whenCalled('getPreview');
    const mediaSizeSettingUpdated = page.getSettingValue('mediaSize');
    assertEquals(
        squareOption.width_microns, mediaSizeSettingUpdated.width_microns);
    assertEquals(
        squareOption.height_microns, mediaSizeSettingUpdated.height_microns);
    const ticket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(squareOption.width_microns, ticket.mediaSize.width_microns);
    assertEquals(squareOption.height_microns, ticket.mediaSize.height_microns);
    nativeLayer.resetResolver('getPreview');
    assertEquals(1, ticket.requestID);
  });

  /** Validate changing the page range updates the preview. */
  test('PageRange', async () => {
    let args = await initialize();
    const originalTicket: PreviewTicket = JSON.parse(args.printTicket);
    // Ranges is empty for full document.
    assertEquals(0, page.getSettingValue('ranges').length);
    assertEquals(0, originalTicket.pageRange.length);
    nativeLayer.resetResolver('getPreview');
    page.setSetting('ranges', [{from: 1, to: 2}]);
    args = await nativeLayer.whenCalled('getPreview');
    const setting = page.getSettingValue('ranges');
    assertEquals(1, setting.length);
    assertEquals(1, setting[0]!.from);
    assertEquals(2, setting[0]!.to);
    const ticket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals(1, ticket.pageRange.length);
    assertEquals(1, ticket.pageRange[0]!.from);
    assertEquals(2, ticket.pageRange[0]!.to);
  });

  /** Validate changing the selection only setting updates the preview. */
  test('SelectionOnly', async () => {
    // Set has selection to true so that the setting is available.
    initialSettings.documentHasSelection = true;
    await testSimpleSetting(
        'selectionOnly', false, true, 'shouldPrintSelectionOnly', false, true);
  });

  /** Validate changing the pages per sheet updates the preview. */
  test('PagesPerSheet', async () => {
    await testSimpleSetting('pagesPerSheet', 1, 2, 'pagesPerSheet', 1, 2);
  });

  /** Validate changing the scaling updates the preview. */
  test('Scaling', async () => {
    let args = await initialize();
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingType',
      expectedTicketId: 0,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> CUSTOM
    page.setSetting('scalingType', ScalingType.CUSTOM);
    // Need to set custom value !== '100' for preview to regenerate.
    page.setSetting('scaling', '90');
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingType',
      expectedTicketId: 1,
      expectedTicketScaleFactor: 90,
      expectedScalingValue: '90',
      expectedScalingType: ScalingType.CUSTOM,
    });
    nativeLayer.resetResolver('getPreview');
    // CUSTOM -> DEFAULT
    // This should regenerate the preview, since the custom value is not
    // 100.
    page.setSetting('scalingType', ScalingType.DEFAULT);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingType',
      expectedTicketId: 2,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '90',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> CUSTOM
    page.setSetting('scalingType', ScalingType.CUSTOM);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingType',
      expectedTicketId: 3,
      expectedTicketScaleFactor: 90,
      expectedScalingValue: '90',
      expectedScalingType: ScalingType.CUSTOM,
    });
    nativeLayer.resetResolver('getPreview');
    // CUSTOM 90 -> CUSTOM 80
    // When custom scaling is set, changing scaling updates preview.
    page.setSetting('scaling', '80');
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingType',
      expectedTicketId: 4,
      expectedTicketScaleFactor: 80,
      expectedScalingValue: '80',
      expectedScalingType: ScalingType.CUSTOM,
    });
  });

  /** Validate changing the scalingTypePdf setting updates the preview. */
  test('ScalingPdf', async () => {
    // Set PDF document so setting is available.
    initialSettings.previewModifiable = false;
    loadTimeData.overrideValues({alignPdfDefaultPrintSettingsWithHTML: false});
    let args = await initialize();
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 0,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> FIT_TO_PAGE
    page.setSetting('scalingTypePdf', ScalingType.FIT_TO_PAGE);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 1,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.FIT_TO_PAGE,
    });
    nativeLayer.resetResolver('getPreview');
    // FIT_TO_PAGE -> CUSTOM
    page.setSetting('scalingTypePdf', ScalingType.CUSTOM);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 2,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.CUSTOM,
    });
    nativeLayer.resetResolver('getPreview');
    // CUSTOM -> FIT_TO_PAGE
    page.setSetting('scalingTypePdf', ScalingType.FIT_TO_PAGE);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 3,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.FIT_TO_PAGE,
    });
    nativeLayer.resetResolver('getPreview');
    // FIT_TO_PAGE -> DEFAULT
    page.setSetting('scalingTypePdf', ScalingType.DEFAULT);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 4,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> FIT_TO_PAPER
    page.setSetting('scalingTypePdf', ScalingType.FIT_TO_PAPER);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 5,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.FIT_TO_PAPER,
    });
    nativeLayer.resetResolver('getPreview');
    // FIT_TO_PAPER -> DEFAULT
    page.setSetting('scalingTypePdf', ScalingType.DEFAULT);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 6,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> CUSTOM
    page.setSetting('scalingTypePdf', ScalingType.CUSTOM);
    // Need to set custom value !== '100' for preview to regenerate.
    page.setSetting('scaling', '120');
    args = await nativeLayer.whenCalled('getPreview');
    // DEFAULT -> CUSTOM will result in a scaling change only if the
    // scale factor changes. Therefore, the requestId should only be one
    // more than the last set of scaling changes.
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 7,
      expectedTicketScaleFactor: 120,
      expectedScalingValue: '120',
      expectedScalingType: ScalingType.CUSTOM,
    });
    nativeLayer.resetResolver('getPreview');
    // CUSTOM -> DEFAULT
    page.setSetting('scalingTypePdf', ScalingType.DEFAULT);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 8,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '120',
      expectedScalingType: ScalingType.DEFAULT,
    });
  });

  /**
   * Validate changing the scalingTypePdf setting updates the preview with
   * alignPdfDefaultPrintSettingsWithHTML on.
   */
  test('ScalingPdfAlignPdfDefaultPrintSettingsWithHTML', async () => {
    // Set PDF document so setting is available.
    initialSettings.previewModifiable = false;
    loadTimeData.overrideValues({alignPdfDefaultPrintSettingsWithHTML: true});
    let args = await initialize();
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 0,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> CUSTOM
    page.setSetting('scalingTypePdf', ScalingType.CUSTOM);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 1,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.CUSTOM,
    });
    nativeLayer.resetResolver('getPreview');
    // CUSTOM -> DEFAULT
    page.setSetting('scalingTypePdf', ScalingType.DEFAULT);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 2,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.DEFAULT,
    });
    nativeLayer.resetResolver('getPreview');
    // DEFAULT -> ACTUAL_SIZE
    page.setSetting('scalingTypePdf', ScalingType.ACTUAL_SIZE);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 3,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '100',
      expectedScalingType: ScalingType.ACTUAL_SIZE,
    });
    nativeLayer.resetResolver('getPreview');
    // ACTUAL_SIZE -> CUSTOM
    page.setSetting('scalingTypePdf', ScalingType.CUSTOM);
    // ACTUAL_SIZE is equal to CUSTOM with scaling value 100, to
    // regenerate preview, we need to make scaling value != 100.
    page.setSetting('scaling', '120');
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 4,
      expectedTicketScaleFactor: 120,
      expectedScalingValue: '120',
      expectedScalingType: ScalingType.CUSTOM,
    });
    nativeLayer.resetResolver('getPreview');
    // CUSTOM -> ACTUAL_SIZE
    page.setSetting('scalingTypePdf', ScalingType.ACTUAL_SIZE);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 5,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '120',
      expectedScalingType: ScalingType.ACTUAL_SIZE,
    });
    nativeLayer.resetResolver('getPreview');
    // ACTUAL_SIZE -> DEFAULT
    page.setSetting('scalingTypePdf', ScalingType.DEFAULT);
    args = await nativeLayer.whenCalled('getPreview');
    validateScalingChange({
      printTicket: args.printTicket,
      scalingTypeKey: 'scalingTypePdf',
      expectedTicketId: 6,
      expectedTicketScaleFactor: 100,
      expectedScalingValue: '120',
      expectedScalingType: ScalingType.DEFAULT,
    });
  });

  /**
   * Validate changing the rasterize setting updates the preview.  Setting is
   * always available on Linux and CrOS.  Availability on Windows and macOS
   * depends upon policy (see policy_test.js).
   */
  test('Rasterize', async () => {
    // Set PDF document so setting is available.
    initialSettings.previewModifiable = false;
    await testSimpleSetting(
        'rasterize', false, true, 'rasterizePDF', false, true);
  });

  /**
   * Validate changing the destination updates the preview, if it results
   * in a settings change.
   */
  test('Destination', async () => {
    let args = await initialize();
    const originalTicket: PreviewTicket = JSON.parse(args.printTicket);
    const destinationSettings =
        page.shadowRoot.querySelector('print-preview-sidebar')!.shadowRoot
            .querySelector('print-preview-destination-settings')!;
    assertTrue(!!destinationSettings.destination);
    assertEquals('FooDevice', destinationSettings.destination.id);
    assertEquals('FooDevice', originalTicket.deviceName);
    const barDestination =
        new Destination('BarDevice', DestinationOrigin.LOCAL, 'BarName');
    const capabilities = getCddTemplate(barDestination.id).capabilities!;
    capabilities.printer.media_size = {
      option: [
        {
          name: 'ISO_A4',
          width_microns: 210000,
          height_microns: 297000,
          custom_display_name: 'A4',
        },
      ],
    };
    barDestination.capabilities = capabilities;
    nativeLayer.resetResolver('getPreview');
    destinationSettings.getDestinationStoreForTest().selectDestination(
        barDestination);
    args = await nativeLayer.whenCalled('getPreview');
    assertTrue(!!destinationSettings.destination);
    assertEquals('BarDevice', destinationSettings.destination.id);
    const ticket: PreviewTicket = JSON.parse(args.printTicket);
    assertEquals('BarDevice', ticket.deviceName);
  });

  /**
   * Validate that if the document layout has 0 default margins, the
   * header/footer setting is set to false.
   */
  test('ZeroDefaultMarginsClearsHeaderFooter', async () => {
    /**
     * @param ticket The parsed print ticket
     * @param expectedId The expected ticket request ID
     * @param expectedMargins The expected ticket margins type
     * @param expectedHeaderFooter The expected ticket
     *     header/footer value
     */
    function assertMarginsFooter(
        ticket: PreviewTicket, expectedId: number, expectedMargins: MarginsType,
        expectedHeaderFooter: boolean) {
      assertEquals(expectedId, ticket.requestID);
      assertEquals(expectedMargins, ticket.marginsType);
      assertEquals(expectedHeaderFooter, ticket.headerFooterEnabled);
    }

    nativeLayer.setPageLayoutInfo({
      marginTop: 0,
      marginLeft: 0,
      marginBottom: 0,
      marginRight: 0,
      contentWidth: 612,
      contentHeight: 792,
      printableAreaX: 0,
      printableAreaY: 0,
      printableAreaWidth: 612,
      printableAreaHeight: 792,
    });

    let previewArgs = await initialize();
    let ticket: PreviewTicket = JSON.parse(previewArgs.printTicket);

    // The ticket recorded here is the original, which requests default
    // margins with headers and footers (Print Preview defaults).
    assertMarginsFooter(ticket, 0, MarginsType.DEFAULT, true);

    // After getting the new layout, a second request should have been
    // sent. Need to wait for a cycle since the 2nd request is issued
    // asynchronously in app.ts.
    await microtasksFinished();
    assertEquals(2, nativeLayer.getCallCount('getPreview'));
    assertEquals(MarginsType.DEFAULT, page.getSettingValue('margins'));
    assertFalse(page.getSettingValue('headerFooter'));

    // Check the last ticket sent by the preview area. It should not
    // have the same settings as the original (headers and footers
    // should have been turned off).
    const previewArea =
        page.shadowRoot.querySelector('print-preview-preview-area')!;
    assertMarginsFooter(
        previewArea.getLastTicketForTest()!, 1, MarginsType.DEFAULT, false);
    nativeLayer.resetResolver('getPreview');
    page.setSetting('margins', MarginsType.MINIMUM);
    previewArgs = await nativeLayer.whenCalled('getPreview');

    // Setting minimum margins allows space for the headers and footers,
    // so they should be enabled again.
    ticket = JSON.parse(previewArgs.printTicket);
    assertMarginsFooter(ticket, 2, MarginsType.MINIMUM, true);
    assertEquals(MarginsType.MINIMUM, page.getSettingValue('margins'));
    assertTrue(page.getSettingValue('headerFooter'));
    nativeLayer.resetResolver('getPreview');
    page.setSetting('margins', MarginsType.DEFAULT);
    previewArgs = await nativeLayer.whenCalled('getPreview');

    // With default margins, there is no space for headers/footers, so
    // they are removed.
    ticket = JSON.parse(previewArgs.printTicket);
    assertMarginsFooter(ticket, 3, MarginsType.DEFAULT, false);
    assertEquals(MarginsType.DEFAULT, page.getSettingValue('margins'));
    assertFalse(page.getSettingValue('headerFooter'));
  });

  /**
   * Validate that the page size calculation handles floating numbers correctly.
   */
  test('PageSizeCalculation', async () => {
    nativeLayer.setPageLayoutInfo({
      marginTop: 28.333,
      marginLeft: 28.333,
      marginBottom: 28.333,
      marginRight: 28.333,
      contentWidth: 555.333,
      contentHeight: 735.333,
      printableAreaX: 0,
      printableAreaY: 0,
      printableAreaWidth: 612,
      printableAreaHeight: 792,
    });

    await initialize();

    assertEquals(612, page.$.documentInfo.pageSize.width);
    assertEquals(792, page.$.documentInfo.pageSize.height);

    const o = CustomMarginsOrientation;
    const margins = page.$.documentInfo.margins;
    assertTrue(!!margins);
    assertEquals(28, margins.get(o.TOP));
    assertEquals(28, margins.get(o.RIGHT));
    assertEquals(28, margins.get(o.BOTTOM));
    assertEquals(28, margins.get(o.LEFT));
  });
});
