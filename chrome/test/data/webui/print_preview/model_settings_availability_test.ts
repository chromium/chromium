// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DuplexOption, MediaSizeOption, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DuplexType, Margins, MarginsType, Size} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getCddTemplate, getSaveAsPdfDestination} from './print_preview_test_utils.js';

suite('ModelSettingsAvailabilityTest', function() {
  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    model.documentSettings = {
      allPagesHaveCustomSize: false,
      allPagesHaveCustomOrientation: false,
      hasSelection: false,
      isFromArc: false,
      isModifiable: true,
      isScalingDisabled: false,
      fitToPageScaling: 100,
      pageCount: 3,
      title: 'title',
    };

    model.pageSize = new Size(612, 792);
    model.margins = new Margins(72, 72, 72, 72);

    // Create a test destination.
    model.destination =
        new Destination('FooDevice', DestinationOrigin.LOCAL, 'FooName');
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
    model.applyStickySettings();
  });

  // These tests verify that the model correctly updates the settings
  // availability based on the destination and document info.
  test('copies', function() {
    assertTrue(model.settings.copies.available);

    // Set max copies to 1.
    let caps = getCddTemplate(model.destination.id).capabilities!;
    const copiesCap = {max: 1};
    caps.printer!.copies = copiesCap;
    model.set('destination.capabilities', caps);
    assertFalse(model.settings.copies.available);

    // Set max copies to 2 (> 1).
    caps = getCddTemplate(model.destination.id).capabilities!;
    copiesCap.max = 2;
    caps.printer!.copies = copiesCap;
    model.set('destination.capabilities', caps);
    assertTrue(model.settings.copies.available);

    // Remove copies capability.
    caps = getCddTemplate(model.destination.id).capabilities!;
    delete caps.printer!.copies;
    model.set('destination.capabilities', caps);
    assertFalse(model.settings.copies.available);

    // Copies is restored.
    caps = getCddTemplate(model.destination.id).capabilities!;
    model.set('destination.capabilities', caps);
    assertTrue(model.settings.copies.available);
    assertFalse(model.settings.copies.setFromUi);
  });

  test('collate', function() {
    assertTrue(model.settings.collate.available);

    // Remove collate capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.collate;
    model.set('destination.capabilities', capabilities);

    // Copies is no longer available.
    assertFalse(model.settings.collate.available);

    // Copies is restored.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    model.set('destination.capabilities', capabilities);
    assertTrue(model.settings.collate.available);
    assertFalse(model.settings.collate.setFromUi);
  });

  test('layout', function() {
    // Layout is available since the printer has the capability and the
    // document is set to modifiable.
    assertTrue(model.settings.layout.available);

    // Each of these settings should not show the capability.
    [undefined,
     {option: [{type: 'PORTRAIT', is_default: true}]},
     {option: [{type: 'LANDSCAPE', is_default: true}]},
    ].forEach(layoutCap => {
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer!.page_orientation = layoutCap;
      // Layout section should now be hidden.
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.layout.available);
    });

    // Reset full capabilities
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    model.set('destination.capabilities', capabilities);
    assertTrue(model.settings.layout.available);

    // Test with PDF - should be hidden.
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.layout.available);

    // Test with ARC - should be available.
    model.set('documentSettings.isFromArc', true);
    assertTrue(model.settings.layout.available);

    model.set('documentSettings.isModifiable', true);
    model.set('documentSettings.isFromArc', false);
    assertTrue(model.settings.layout.available);

    // Unavailable if all pages have specified an orientation.
    model.set('documentSettings.allPagesHaveCustomOrientation', true);
    assertFalse(model.settings.layout.available);
    assertFalse(model.settings.layout.setFromUi);
  });

  test('color', function() {
    // Color is available since the printer has the capability.
    assertTrue(model.settings.color.available);

    // Each of these settings should make the setting unavailable, with
    // |expectedValue| as its unavailableValue.
    [{
      colorCap: undefined,
      expectedValue: false,
    },
     {
       colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
       expectedValue: true,
     },
     {
       colorCap: {
         option: [
           {type: 'STANDARD_COLOR', is_default: true},
           {type: 'CUSTOM_COLOR'},
         ],
       },
       expectedValue: true,
     },
     {
       colorCap: {
         option: [
           {type: 'STANDARD_MONOCHROME', is_default: true},
           {type: 'CUSTOM_MONOCHROME'},
         ],
       },
       expectedValue: false,
     },
     {
       colorCap: {option: [{type: 'STANDARD_MONOCHROME'}]},
       expectedValue: false,
     },
     {
       colorCap: {option: [{type: 'CUSTOM_MONOCHROME', vendor_id: '42'}]},
       expectedValue: false,
     },
     {
       colorCap: {option: [{type: 'CUSTOM_COLOR', vendor_id: '42'}]},
       expectedValue: true,
     }].forEach(capabilityAndValue => {
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer!.color = capabilityAndValue.colorCap;
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.color.available);
      assertEquals(
          capabilityAndValue.expectedValue,
          model.settings.color.unavailableValue as boolean);
    });

    // Each of these settings should make the setting available, with the
    // default value given by expectedValue.
    [{
      colorCap: {
        option: [
          {type: 'STANDARD_MONOCHROME', is_default: true},
          {type: 'STANDARD_COLOR'},
        ],
      },
      expectedValue: false,
    },
     {
       colorCap: {
         option: [
           {type: 'STANDARD_MONOCHROME'},
           {type: 'STANDARD_COLOR', is_default: true},
         ],
       },
       expectedValue: true,
     },
     {
       colorCap: {
         option: [
           {type: 'CUSTOM_MONOCHROME', vendor_id: '42'},
           {type: 'CUSTOM_COLOR', is_default: true, vendor_id: '43'},
         ],
       },
       expectedValue: true,
     }].forEach(capabilityAndValue => {
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer!.color = capabilityAndValue.colorCap;
      model.set('destination.capabilities', capabilities);
      assertEquals(
          capabilityAndValue.expectedValue, model.settings.color.value);
      assertTrue(model.settings.color.available);
    });
  });

  function setSaveAsPdfDestination() {
    const saveAsPdf = getSaveAsPdfDestination();
    saveAsPdf.capabilities = getCddTemplate(model.destination.id).capabilities;
    model.set('destination', saveAsPdf);
  }

  test('media size', function() {
    // Media size is available since the printer has the capability.
    assertTrue(model.settings.mediaSize.available);

    // Remove capability.
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.media_size;

    // Section should now be hidden.
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.mediaSize.available);

    // Set Save as PDF printer.
    setSaveAsPdfDestination();

    // Save as PDF printer has media size capability.
    assertTrue(model.settings.mediaSize.available);

    // PDF to PDF -> media size is unavailable.
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.mediaSize.available);
    model.set('documentSettings.isModifiable', true);

    // Even if all pages have specified their orientation, the size option
    // should still be available.
    model.set('documentSettings.allPagesHaveCustomOrientation', true);
    assertTrue(model.settings.mediaSize.available);
    model.set('documentSettings.allPagesHaveCustomOrientation', false);

    // If all pages have specified a size, the size option shouldn't be
    // available.
    model.set('documentSettings.allPagesHaveCustomSize', true);
    assertFalse(model.settings.mediaSize.available);
    assertFalse(model.settings.color.setFromUi);
  });

  test('borderless', function() {
    // Check that borderless setting is unavailable without the feature flag.
    loadTimeData.overrideValues({isBorderlessPrintingEnabled: false});
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
    assertFalse(model.settings.borderless.available);

    // Enable the feature flag and set capabilities again to update borderless
    // availability.
    loadTimeData.overrideValues({isBorderlessPrintingEnabled: true});
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
    assertTrue(model.settings.borderless.available);

    // Remove the only media size with a borderless variant.
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer!.media_size!.option.splice(1, 1);
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.borderless.available);
  });

  test('mediaType', function() {
    // Check that media type setting is unavailable without the feature flag.
    loadTimeData.overrideValues({isBorderlessPrintingEnabled: false});
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
    assertFalse(model.settings.mediaType.available);

    // Enable the feature flag and set capabilities again to update media type
    // availability.
    loadTimeData.overrideValues({isBorderlessPrintingEnabled: true});
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
    assertTrue(model.settings.mediaType.available);

    // Remove media type capability.
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.media_type;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.mediaType.available);
  });

  test('margins', function() {
    // The settings are available since isModifiable is true.
    assertTrue(model.settings.margins.available);
    assertTrue(model.settings.customMargins.available);

    // No margins settings for ARC.
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.margins.available);
    assertFalse(model.settings.customMargins.available);
    assertFalse(model.settings.margins.setFromUi);
    assertFalse(model.settings.customMargins.setFromUi);

    // No margins settings for PDFs.
    model.set('documentSettings.isFromArc', false);
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.margins.available);
    assertFalse(model.settings.customMargins.available);
    assertFalse(model.settings.margins.setFromUi);
    assertFalse(model.settings.customMargins.setFromUi);
  });

  test('dpi', function() {
    // The settings are available since the printer has multiple DPI options.
    assertTrue(model.settings.dpi.available);

    // No resolution settings for ARC, but uses the default value.
    model.set('documentSettings.isFromArc', true);
    let capabilities = getCddTemplate(model.destination.id).capabilities;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.dpi.available);
    assertEquals(200, model.settings.dpi.unavailableValue.horizontal_dpi);
    assertEquals(200, model.settings.dpi.unavailableValue.vertical_dpi);

    model.set('documentSettings.isFromArc', false);
    assertTrue(model.settings.dpi.available);

    // Remove capability.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.dpi;

    // Section should now be hidden.
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.dpi.available);

    // Does not show up for only 1 option. Unavailable value should be set to
    // the only available option.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer!.dpi!.option.pop();
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.dpi.available);
    assertEquals(200, model.settings.dpi.unavailableValue.horizontal_dpi);
    assertEquals(200, model.settings.dpi.unavailableValue.vertical_dpi);
    assertFalse(model.settings.dpi.setFromUi);
  });

  test('scaling', function() {
    // HTML -> printer
    assertTrue(model.settings.scaling.available);

    // HTML -> Save as PDF
    const defaultDestination = model.destination;
    setSaveAsPdfDestination();
    assertTrue(model.settings.scaling.available);

    // PDF -> Save as PDF
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.scaling.available);

    // PDF -> printer
    model.set('destination', defaultDestination);
    assertTrue(model.settings.scaling.available);
    assertFalse(model.settings.scaling.setFromUi);

    // ARC -> printer
    model.set('destination', defaultDestination);
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.scaling.available);

    // ARC -> Save as PDF
    setSaveAsPdfDestination();
    assertFalse(model.settings.scaling.available);
  });

  test('scalingType', function() {
    // HTML -> printer
    assertTrue(model.settings.scalingType.available);

    // HTML -> Save as PDF
    const defaultDestination = model.destination;
    setSaveAsPdfDestination();
    assertTrue(model.settings.scalingType.available);

    // PDF -> Save as PDF
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.scalingType.available);

    // PDF -> printer
    model.set('destination', defaultDestination);
    assertFalse(model.settings.scalingType.available);

    // ARC -> printer
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.scalingType.available);

    // ARC -> Save as PDF
    setSaveAsPdfDestination();
    assertFalse(model.settings.scalingType.available);
  });

  test('scalingTypePdf', function() {
    // HTML -> printer
    assertFalse(model.settings.scalingTypePdf.available);

    // HTML -> Save as PDF
    const defaultDestination = model.destination;
    setSaveAsPdfDestination();
    assertFalse(model.settings.scalingTypePdf.available);

    // PDF -> Save as PDF
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.scalingTypePdf.available);

    // PDF -> printer
    model.set('destination', defaultDestination);
    assertTrue(model.settings.scalingTypePdf.available);

    // ARC -> printer
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.scalingTypePdf.available);

    // ARC -> Save as PDF
    setSaveAsPdfDestination();
    assertFalse(model.settings.scalingTypePdf.available);
  });

  test('header footer', function() {
    // Default margins + letter paper + HTML page.
    assertTrue(model.settings.headerFooter.available);

    // Custom margins initializes with customMargins undefined and margins
    // values matching the defaults.
    model.set('settings.margins.value', MarginsType.CUSTOM);
    assertTrue(model.settings.headerFooter.available);

    // Set margins to NONE
    model.set('settings.margins.value', MarginsType.NO_MARGINS);
    assertFalse(model.settings.headerFooter.available);

    // Set margins to MINIMUM
    model.set('settings.margins.value', MarginsType.MINIMUM);
    assertTrue(model.settings.headerFooter.available);

    // Custom margins of 0.
    model.set('settings.margins.value', MarginsType.CUSTOM);
    model.set(
        'settings.customMargins.value',
        {marginTop: 0, marginLeft: 0, marginRight: 0, marginBottom: 0});
    model.set('margins', new Margins(0, 0, 0, 0));
    assertFalse(model.settings.headerFooter.available);

    // Custom margins of 36 -> header/footer available
    model.set(
        'settings.customMargins.value',
        {marginTop: 36, marginLeft: 36, marginRight: 36, marginBottom: 36});
    model.set('margins', new Margins(36, 36, 36, 36));
    assertTrue(model.settings.headerFooter.available);

    // Zero top and bottom -> header/footer unavailable
    model.set(
        'settings.customMargins.value',
        {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 0});
    model.set('margins', new Margins(0, 36, 0, 36));
    assertFalse(model.settings.headerFooter.available);

    // Zero top and nonzero bottom -> header/footer available
    model.set(
        'settings.customMargins.value',
        {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 36});
    model.set('margins', new Margins(0, 36, 36, 36));
    assertTrue(model.settings.headerFooter.available);

    // Small paper sizes
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer!.media_size = {
      'option': [
        {
          'name': 'SmallLabel',
          'width_microns': 38100,
          'height_microns': 12700,
          'is_default': false,
        },
        {
          'name': 'BigLabel',
          'width_microns': 50800,
          'height_microns': 76200,
          'is_default': true,
        },
      ] as MediaSizeOption[],
    };
    model.set('destination.capabilities', capabilities);
    model.set('settings.margins.value', MarginsType.DEFAULT);

    // Header/footer should be available for default big label with
    // default margins.
    assertTrue(model.settings.headerFooter.available);

    model.set(
        'settings.mediaSize.value', capabilities.printer.media_size!.option[0]);

    // Header/footer should not be available for small label
    assertFalse(model.settings.headerFooter.available);

    // Reset to big label.
    model.set(
        'settings.mediaSize.value', capabilities.printer.media_size!.option[1]);
    assertTrue(model.settings.headerFooter.available);

    // Header/footer is never available for PDFs.
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.headerFooter.available);
    assertFalse(model.settings.headerFooter.setFromUi);

    // Header/footer is never available for ARC.
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.headerFooter.available);
    assertFalse(model.settings.headerFooter.setFromUi);
  });

  test('css background', function() {
    // The setting is available since isModifiable is true.
    assertTrue(model.settings.cssBackground.available);

    // No CSS background setting for PDFs.
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.cssBackground.available);
    assertFalse(model.settings.cssBackground.setFromUi);

    // No CSS background setting for ARC.
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.cssBackground.available);
    assertFalse(model.settings.cssBackground.setFromUi);
  });

  test('duplex', function() {
    assertTrue(model.settings.duplex.available);
    assertTrue(model.settings.duplexShortEdge.available);

    // Remove duplex capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.duplex;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.duplex.available);
    assertFalse(model.settings.duplexShortEdge.available);

    // Set a duplex capability with only 1 type, no duplex.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.duplex;
    capabilities.printer.duplex = {
      option: [{type: DuplexType.NO_DUPLEX, is_default: true}],
    };
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.duplex.available);
    assertFalse(model.settings.duplexShortEdge.available);

    // Set a duplex capability with 2 types, long edge and no duplex.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.duplex;
    capabilities.printer.duplex = {
      option: [
        {type: DuplexType.NO_DUPLEX},
        {type: DuplexType.LONG_EDGE, is_default: true},
      ] as DuplexOption[],
    };
    model.set('destination.capabilities', capabilities);
    assertTrue(model.settings.duplex.available);
    assertFalse(model.settings.duplexShortEdge.available);
    assertFalse(model.settings.duplex.setFromUi);
    assertFalse(model.settings.duplexShortEdge.setFromUi);
  });

  test('rasterize', function() {
    // Availability for PDFs varies depening upon OS.
    // Windows and macOS depend on policy - see policy_test.js for their
    // testing coverage.
    model.set('documentSettings.isModifiable', false);
    // <if expr="is_linux or is_chromeos">
    // Always available for PDFs on Linux and ChromeOS
    assertTrue(model.settings.rasterize.available);
    assertFalse(model.settings.rasterize.setFromUi);
    // </if>

    // Unavailable for ARC.
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.rasterize.available);
  });

  test('selection only', function() {
    // Not available with no selection.
    assertFalse(model.settings.selectionOnly.available);

    model.set('documentSettings.hasSelection', true);
    assertTrue(model.settings.selectionOnly.available);

    // Not available for PDFs.
    model.set('documentSettings.isModifiable', false);
    assertFalse(model.settings.selectionOnly.available);
    assertFalse(model.settings.selectionOnly.setFromUi);

    // Not available for ARC.
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.selectionOnly.available);
    assertFalse(model.settings.selectionOnly.setFromUi);
  });

  test('pages per sheet', function() {
    // Pages per sheet is available everywhere except for ARC.
    // With the default settings for Blink content, it is available.
    model.set('documentSettings.isModifiable', true);
    assertTrue(model.settings.pagesPerSheet.available);

    // Still available for PDF content.
    model.set('documentSettings.isModifiable', false);
    assertTrue(model.settings.pagesPerSheet.available);

    // Not available for ARC.
    model.set('documentSettings.isFromArc', true);
    assertFalse(model.settings.pagesPerSheet.available);
  });

  // <if expr="is_chromeos">
  test('pin', function() {
    // Make device unmanaged.
    loadTimeData.overrideValues({isEnterpriseManaged: false});
    // Check that pin setting is unavailable on unmanaged devices.
    assertFalse(model.settings.pin.available);

    // Make device enterprise managed.
    loadTimeData.overrideValues({isEnterpriseManaged: true});
    // Set capabilities again to update pin availability.
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
    assertTrue(model.settings.pin.available);

    // Remove pin capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer!.pin;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.pin.available);

    // Set not supported pin capability.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer!.pin!.supported = false;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.pin.available);
    assertFalse(model.settings.pin.setFromUi);
  });

  test('pinValue', function() {
    assertTrue(model.settings.pinValue.available);

    // Remove pin capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.pin;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.pinValue.available);

    // Set not supported pin capability.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer.pin!.supported = false;
    model.set('destination.capabilities', capabilities);
    assertFalse(model.settings.pinValue.available);
    assertFalse(model.settings.pinValue.setFromUi);
  });
  // </if>
});
