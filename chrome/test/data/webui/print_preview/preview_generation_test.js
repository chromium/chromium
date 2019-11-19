// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorMode, Destination, DestinationConnectionStatus, DestinationOrigin, DestinationState, DestinationType, Margins, MarginsType, NativeLayer, PluginProxy, ScalingType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplate, getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.preview_generation_test = {};
preview_generation_test.suiteName = 'PreviewGenerationTest';
/** @enum {string} */
preview_generation_test.TestNames = {
  Color: 'color',
  CssBackground: 'css background',
  HeaderFooter: 'header/footer',
  Layout: 'layout',
  Margins: 'margins',
  CustomMargins: 'custom margins',
  MediaSize: 'media size',
  PageRange: 'page range',
  Rasterize: 'rasterize',
  PagesPerSheet: 'pages per sheet',
  Scaling: 'scaling',
  ScalingPdf: 'scalingPdf',
  SelectionOnly: 'selection only',
  Destination: 'destination',
  ChangeMarginsByPagesPerSheet: 'change margins by pages per sheet',
  ZeroDefaultMarginsClearsHeaderFooter:
      'zero default margins clears header/footer',
};

/**
 * @typedef {{
 *   printTicket: !Object,
 *   scalingTypeKey: string,
 *   expectedTicketId: number,
 *   expectedTicketScaleFactor: number,
 *   expectedScalingValue: string,
 *   expectedScalingType: !ScalingType,
 * }}
 */
let ValidateScalingChangeParams;

suite(preview_generation_test.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {!NativeInitialSettings} */
  const initialSettings = getDefaultInitialSettings();

  /** @override */
  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);
    PolymerTest.clearBody();
  });

  /**
   * Initializes the UI with a default local destination and a 3 page document
   * length.
   * @return {!Promise} Promise that resolves when initialization is done,
   *     destination is set, and initial preview request is complete.
   */
  function initialize() {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate(initialSettings.printerName));
    nativeLayer.setPageCount(3);
    const pluginProxy = new PDFPluginStub();
    PluginProxy.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    const previewArea = page.$.previewArea;
    const documentInfo = page.$$('print-preview-document-info');
    documentInfo.documentSettings.pageCount = 3;
    documentInfo.margins = new Margins(10, 10, 10, 10);

    return Promise
        .all([
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities'),
        ])
        .then(function() {
          if (!documentInfo.documentSettings.isModifiable) {
            documentInfo.documentSettings.fitToPageScaling = 98;
          }
          return nativeLayer.whenCalled('getPreview');
        });
  }

  /**
   * @param {string} settingName The name of the setting to test.
   * @param {boolean | string} initialSettingValue The default setting value.
   * @param {boolean | string} updatedSettingValue The setting value to update
   *     to.
   * @param {string} ticketKey The field in the print ticket that corresponds
   *     to the setting.
   * @param {boolean | string | number} initialTicketValue The ticket value
   *     corresponding to the default setting value.
   * @param {boolean | string | number} updatedTicketValue The ticket value
   *     corresponding to the updated setting value.
   * @return {!Promise} Promise that resolves when the setting has been
   *     changed, the preview has been regenerated, and the print ticket and
   *     UI state have been verified.
   */
  function testSimpleSetting(
      settingName, initialSettingValue, updatedSettingValue, ticketKey,
      initialTicketValue, updatedTicketValue) {
    return initialize()
        .then(function(args) {
          const originalTicket = JSON.parse(args.printTicket);
          assertEquals(0, originalTicket.requestID);
          assertEquals(initialTicketValue, originalTicket[ticketKey]);
          nativeLayer.resetResolver('getPreview');
          assertEquals(initialSettingValue, page.getSettingValue(settingName));
          page.setSetting(settingName, updatedSettingValue);
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          assertEquals(updatedSettingValue, page.getSettingValue(settingName));
          const ticket = JSON.parse(args.printTicket);
          assertEquals(updatedTicketValue, ticket[ticketKey]);
          assertEquals(1, ticket.requestID);
        });
  }

  /** @param {ValidateScalingChangeParams} input Test arguments. */
  function validateScalingChange(input) {
    const ticket = JSON.parse(input.printTicket);
    assertEquals(input.expectedTicketId, ticket.requestID);
    assertEquals(input.expectedTicketScaleFactor, ticket.scaleFactor);
    assertEquals(input.expectedScalingValue, page.getSettingValue('scaling'));
    assertEquals(
        input.expectedScalingType, page.getSettingValue(input.scalingTypeKey));
  }

  /** Validate changing the color updates the preview. */
  test(assert(preview_generation_test.TestNames.Color), function() {
    return testSimpleSetting(
        'color', true, false, 'color', ColorMode.COLOR, ColorMode.GRAY);
  });

  /** Validate changing the background setting updates the preview. */
  test(assert(preview_generation_test.TestNames.CssBackground), function() {
    return testSimpleSetting(
        'cssBackground', false, true, 'shouldPrintBackgrounds', false, true);
  });

  /** Validate changing the header/footer setting updates the preview. */
  test(assert(preview_generation_test.TestNames.HeaderFooter), function() {
    return testSimpleSetting(
        'headerFooter', true, false, 'headerFooterEnabled', true, false);
  });

  /** Validate changing the orientation updates the preview. */
  test(assert(preview_generation_test.TestNames.Layout), function() {
    return testSimpleSetting('layout', false, true, 'landscape', false, true);
  });

  /** Validate changing the margins updates the preview. */
  test(assert(preview_generation_test.TestNames.Margins), function() {
    return testSimpleSetting(
        'margins', MarginsType.DEFAULT, MarginsType.MINIMUM, 'marginsType',
        MarginsType.DEFAULT, MarginsType.MINIMUM);
  });

  /**
   * Validate changing the custom margins updates the preview, only after all
   * values have been set.
   */
  test(assert(preview_generation_test.TestNames.CustomMargins), function() {
    return initialize()
        .then(function(args) {
          const originalTicket = JSON.parse(args.printTicket);
          assertEquals(MarginsType.DEFAULT, originalTicket.marginsType);
          // Custom margins should not be set in the ticket.
          assertEquals(undefined, originalTicket.marginsCustom);
          assertEquals(0, originalTicket.requestID);

          // This should do nothing.
          page.setSetting('margins', MarginsType.CUSTOM);
          // Sets only 1 side, not valid.
          page.setSetting('customMargins', {marginTop: 25});
          // 2 sides, still not valid.
          page.setSetting('customMargins', {marginTop: 25, marginRight: 40});
          // This should trigger a preview.
          nativeLayer.resetResolver('getPreview');
          page.setSetting('customMargins', {
            marginTop: 25,
            marginRight: 40,
            marginBottom: 20,
            marginLeft: 50
          });
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          const ticket = JSON.parse(args.printTicket);
          assertEquals(MarginsType.CUSTOM, ticket.marginsType);
          assertEquals(25, ticket.marginsCustom.marginTop);
          assertEquals(40, ticket.marginsCustom.marginRight);
          assertEquals(20, ticket.marginsCustom.marginBottom);
          assertEquals(50, ticket.marginsCustom.marginLeft);
          assertEquals(1, ticket.requestID);
          page.setSetting('margins', MarginsType.DEFAULT);
          // Set setting to something invalid and then set margins to CUSTOM.
          page.setSetting('customMargins', {marginTop: 25, marginRight: 40});
          page.setSetting('margins', MarginsType.CUSTOM);
          nativeLayer.resetResolver('getPreview');
          page.setSetting('customMargins', {
            marginTop: 25,
            marginRight: 40,
            marginBottom: 20,
            marginLeft: 50
          });
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          const ticket = JSON.parse(args.printTicket);
          assertEquals(MarginsType.CUSTOM, ticket.marginsType);
          assertEquals(25, ticket.marginsCustom.marginTop);
          assertEquals(40, ticket.marginsCustom.marginRight);
          assertEquals(20, ticket.marginsCustom.marginBottom);
          assertEquals(50, ticket.marginsCustom.marginLeft);
          // Request 3. Changing to default margins should have triggered a
          // preview, and the final setting of valid custom margins should
          // have triggered another one.
          assertEquals(3, ticket.requestID);
        });
  });

  /**
   * Validate changing the pages per sheet updates the preview, and resets
   * margins to MarginsType.DEFAULT.
   */
  test(
      assert(preview_generation_test.TestNames.ChangeMarginsByPagesPerSheet),
      function() {
        return initialize()
            .then(function(args) {
              const originalTicket = JSON.parse(args.printTicket);
              assertEquals(0, originalTicket.requestID);
              assertEquals(MarginsType.DEFAULT, originalTicket['marginsType']);
              assertEquals(
                  MarginsType.DEFAULT, page.getSettingValue('margins'));
              assertEquals(1, page.getSettingValue('pagesPerSheet'));
              assertEquals(1, originalTicket['pagesPerSheet']);
              nativeLayer.resetResolver('getPreview');
              page.setSetting('margins', MarginsType.MINIMUM);
              return nativeLayer.whenCalled('getPreview');
            })
            .then(function(args) {
              assertEquals(
                  MarginsType.MINIMUM, page.getSettingValue('margins'));
              const ticket = JSON.parse(args.printTicket);
              assertEquals(MarginsType.MINIMUM, ticket['marginsType']);
              nativeLayer.resetResolver('getPreview');
              assertEquals(1, ticket.requestID);
              page.setSetting('pagesPerSheet', 4);
              return nativeLayer.whenCalled('getPreview');
            })
            .then(function(args) {
              assertEquals(
                  MarginsType.DEFAULT, page.getSettingValue('margins'));
              assertEquals(4, page.getSettingValue('pagesPerSheet'));
              const ticket = JSON.parse(args.printTicket);
              assertEquals(MarginsType.DEFAULT, ticket['marginsType']);
              assertEquals(4, ticket['pagesPerSheet']);
              assertEquals(2, ticket.requestID);
            });
      });

  /** Validate changing the paper size updates the preview. */
  test(assert(preview_generation_test.TestNames.MediaSize), function() {
    const mediaSizeCapability =
        getCddTemplate('FooDevice').capabilities.printer.media_size;
    const letterOption = mediaSizeCapability.option[0];
    const squareOption = mediaSizeCapability.option[1];
    return initialize()
        .then(function(args) {
          const originalTicket = JSON.parse(args.printTicket);
          assertEquals(
              letterOption.width_microns,
              originalTicket.mediaSize.width_microns);
          assertEquals(
              letterOption.height_microns,
              originalTicket.mediaSize.height_microns);
          assertEquals(0, originalTicket.requestID);
          nativeLayer.resetResolver('getPreview');
          const mediaSizeSetting = page.getSettingValue('mediaSize');
          assertEquals(
              letterOption.width_microns, mediaSizeSetting.width_microns);
          assertEquals(
              letterOption.height_microns, mediaSizeSetting.height_microns);
          page.setSetting('mediaSize', squareOption);
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          const mediaSizeSettingUpdated = page.getSettingValue('mediaSize');
          assertEquals(
              squareOption.width_microns,
              mediaSizeSettingUpdated.width_microns);
          assertEquals(
              squareOption.height_microns,
              mediaSizeSettingUpdated.height_microns);
          const ticket = JSON.parse(args.printTicket);
          assertEquals(
              squareOption.width_microns, ticket.mediaSize.width_microns);
          assertEquals(
              squareOption.height_microns, ticket.mediaSize.height_microns);
          nativeLayer.resetResolver('getPreview');
          assertEquals(1, ticket.requestID);
        });
  });

  /** Validate changing the page range updates the preview. */
  test(assert(preview_generation_test.TestNames.PageRange), function() {
    return initialize()
        .then(function(args) {
          const originalTicket = JSON.parse(args.printTicket);
          // Ranges is empty for full document.
          assertEquals(0, page.getSettingValue('ranges').length);
          assertEquals(0, originalTicket.pageRange.length);
          nativeLayer.resetResolver('getPreview');
          page.setSetting('ranges', [{from: 1, to: 2}]);
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          const setting = page.getSettingValue('ranges');
          assertEquals(1, setting.length);
          assertEquals(1, setting[0].from);
          assertEquals(2, setting[0].to);
          const ticket = JSON.parse(args.printTicket);
          assertEquals(1, ticket.pageRange.length);
          assertEquals(1, ticket.pageRange[0].from);
          assertEquals(2, ticket.pageRange[0].to);
        });
  });

  /** Validate changing the selection only setting updates the preview. */
  test(assert(preview_generation_test.TestNames.SelectionOnly), function() {
    // Set has selection to true so that the setting is available.
    initialSettings.hasSelection = true;
    return testSimpleSetting(
        'selectionOnly', false, true, 'shouldPrintSelectionOnly', false, true);
  });

  /** Validate changing the pages per sheet updates the preview. */
  test(assert(preview_generation_test.TestNames.PagesPerSheet), function() {
    return testSimpleSetting('pagesPerSheet', 1, 2, 'pagesPerSheet', 1, 2);
  });

  /** Validate changing the scaling updates the preview. */
  test(assert(preview_generation_test.TestNames.Scaling), function() {
    return initialize()
        .then(function(args) {
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
          // Need to set custom value != 100 for preview to regenerate.
          page.setSetting('scaling', '90');
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          validateScalingChange({
            printTicket: args.printTicket,
            scalingTypeKey: 'scalingType',
            expectedTicketId: 4,
            expectedTicketScaleFactor: 80,
            expectedScalingValue: '80',
            expectedScalingType: ScalingType.CUSTOM,
          });
        });
  });

  /** Validate changing the scalingTypePdf setting updates the preview. */
  test(assert(preview_generation_test.TestNames.ScalingPdf), function() {
    // Set PDF document so setting is available.
    initialSettings.previewModifiable = false;
    initialSettings.previewIsPdf = true;
    return initialize()
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          // Need to set custom value != 100 for preview to regenerate.
          page.setSetting('scaling', '120');
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
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
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          validateScalingChange({
            printTicket: args.printTicket,
            scalingTypeKey: 'scalingTypePdf',
            expectedTicketId: 8,
            expectedTicketScaleFactor: 100,
            expectedScalingValue: '120',
            expectedScalingType: ScalingType.DEFAULT,
          });
        });
  });

  /**
   * Validate changing the rasterize setting updates the preview. Only runs
   * on Linux and CrOS as setting is not available on other platforms.
   */
  test(assert(preview_generation_test.TestNames.Rasterize), function() {
    // Set PDF document so setting is available.
    initialSettings.previewModifiable = false;
    return testSimpleSetting(
        'rasterize', false, true, 'rasterizePDF', false, true);
  });

  /**
   * Validate changing the destination updates the preview, if it results
   * in a settings change.
   */
  test(assert(preview_generation_test.TestNames.Destination), function() {
    return initialize()
        .then(function(args) {
          const originalTicket = JSON.parse(args.printTicket);
          assertEquals('FooDevice', page.destination_.id);
          assertEquals('FooDevice', originalTicket.deviceName);
          const barDestination = new Destination(
              'BarDevice', DestinationType.LOCAL, DestinationOrigin.LOCAL,
              'BarName', DestinationConnectionStatus.ONLINE);
          const capabilities = getCddTemplate(barDestination.id).capabilities;
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
          page.destinationState_ = DestinationState.SELECTED;
          page.set('destination_', barDestination);
          page.destinationState_ = DestinationState.UPDATED;
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function(args) {
          assertEquals('BarDevice', page.destination_.id);
          const ticket = JSON.parse(args.printTicket);
          assertEquals('BarDevice', ticket.deviceName);
        });
  });

  /**
   * Validate that if the document layout has 0 default margins, the
   * header/footer setting is set to false.
   */
  test(
      assert(preview_generation_test.TestNames
                 .ZeroDefaultMarginsClearsHeaderFooter),
      async () => {
        /**
         * @param {Object} ticket The parsed print ticket
         * @param {number} expectedId The expected ticket request ID
         * @param {!MarginsType} expectedMargins
         *     The expected ticket margins type
         * @param {boolean} expectedHeaderFooter The expected ticket
         *     header/footer value
         */
        const assertMarginsFooter = function(
            ticket, expectedId, expectedMargins, expectedHeaderFooter) {
          assertEquals(expectedId, ticket.requestID);
          assertEquals(expectedMargins, ticket.marginsType);
          assertEquals(expectedHeaderFooter, ticket.headerFooterEnabled);
        };

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
        let ticket = JSON.parse(previewArgs.printTicket);

        // The ticket recorded here is the original, which requests default
        // margins with headers and footers (Print Preview defaults).
        assertMarginsFooter(ticket, 0, MarginsType.DEFAULT, true);

        // After getting the new layout, a second request should have been
        // sent.
        assertEquals(2, nativeLayer.getCallCount('getPreview'));
        assertEquals(MarginsType.DEFAULT, page.getSettingValue('margins'));
        assertFalse(page.getSettingValue('headerFooter'));

        // Check the last ticket sent by the preview area. It should not
        // have the same settings as the original (headers and footers
        // should have been turned off).
        const previewArea = page.$$('print-preview-preview-area');
        assertMarginsFooter(
            previewArea.lastTicket_, 1, MarginsType.DEFAULT, false);
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
        assertEquals(false, page.getSettingValue('headerFooter'));
      });
});
