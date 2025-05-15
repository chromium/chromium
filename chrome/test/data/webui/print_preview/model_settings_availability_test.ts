// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {Cdd, DuplexOption, MediaSizeOption, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DuplexType, Margins, MarginsType, Size} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDocumentSettings, getCddTemplate, getSaveAsPdfDestination} from './print_preview_test_utils.js';

suite('ModelSettingsAvailabilityTest', function() {
  let model: PrintPreviewModelElement;

  function simulateCapabilitiesChange(capabilities: Cdd) {
    model.destination.capabilities = capabilities;
    // In prod code, capabilities changes are detected by print-preview-app
    // which then calls updateSettingsFromDestination().
    model.updateSettingsFromDestination();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    model.documentSettings = createDocumentSettings({
      pageCount: 3,
      title: 'title',
    });
    model.pageSize = new Size(612, 792);
    model.margins = new Margins(72, 72, 72, 72);

    // Create a test destination.
    model.destination =
        new Destination('FooDevice', DestinationOrigin.LOCAL, 'FooName');
    simulateCapabilitiesChange(
        getCddTemplate(model.destination.id).capabilities!);
    model.applyStickySettings();
    return microtasksFinished();
  });

  // These tests verify that the model correctly updates the settings
  // availability based on the destination and document info.
  test('copies', function() {
    assertTrue(model.settings.copies.available);

    // Set max copies to 1.
    let caps = getCddTemplate(model.destination.id).capabilities!;
    const copiesCap = {max: 1};
    caps.printer.copies = copiesCap;
    simulateCapabilitiesChange(caps);
    assertFalse(model.settings.copies.available);

    // Set max copies to 2 (> 1).
    caps = getCddTemplate(model.destination.id).capabilities!;
    copiesCap.max = 2;
    caps.printer.copies = copiesCap;
    simulateCapabilitiesChange(caps);
    assertTrue(model.settings.copies.available);

    // Remove copies capability.
    caps = getCddTemplate(model.destination.id).capabilities!;
    delete caps.printer.copies;
    simulateCapabilitiesChange(caps);
    assertFalse(model.settings.copies.available);

    // Copies is restored.
    caps = getCddTemplate(model.destination.id).capabilities!;
    simulateCapabilitiesChange(caps);
    assertTrue(model.settings.copies.available);
    assertFalse(model.settings.copies.setFromUi);
  });

  test('collate', function() {
    assertTrue(model.settings.collate.available);

    // Remove collate capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.collate;
    simulateCapabilitiesChange(capabilities);

    // Copies is no longer available.
    assertFalse(model.settings.collate.available);

    // Copies is restored.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    simulateCapabilitiesChange(capabilities);
    assertTrue(model.settings.collate.available);
    assertFalse(model.settings.collate.setFromUi);
  });

  test('layout', async function() {
    // Layout is available since the printer has the capability and the
    // document is set to modifiable.
    assertTrue(model.settings.layout.available);

    // Each of these settings should not show the capability.
    [undefined,
     {option: [{type: 'PORTRAIT', is_default: true}]},
     {option: [{type: 'LANDSCAPE', is_default: true}]},
    ].forEach(layoutCap => {
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer.page_orientation = layoutCap;
      // Layout section should now be hidden.
      simulateCapabilitiesChange(capabilities);
      assertFalse(model.settings.layout.available);
    });

    // Reset full capabilities
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    simulateCapabilitiesChange(capabilities);
    assertTrue(model.settings.layout.available);

    // Test with PDF - should be hidden.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.layout.available);

    // Unavailable if all pages have specified an orientation.
    model.documentSettings = createDocumentSettings(
        model.documentSettings, {allPagesHaveCustomOrientation: true});
    await microtasksFinished();
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
      capabilities.printer.color = capabilityAndValue.colorCap;
      simulateCapabilitiesChange(capabilities);
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
      capabilities.printer.color = capabilityAndValue.colorCap;
      simulateCapabilitiesChange(capabilities);
      assertEquals(
          capabilityAndValue.expectedValue, model.settings.color.value);
      assertTrue(model.settings.color.available);
    });
  });

  function setSaveAsPdfDestination(): Promise<void> {
    const saveAsPdf = getSaveAsPdfDestination();
    saveAsPdf.capabilities = getCddTemplate(model.destination.id).capabilities;
    model.destination = saveAsPdf;
    return microtasksFinished();
  }

  test('media size', async function() {
    // Media size is available since the printer has the capability.
    assertTrue(model.settings.mediaSize.available);

    // Remove capability.
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.media_size;

    // Section should now be hidden.
    simulateCapabilitiesChange(capabilities);
    assertFalse(model.settings.mediaSize.available);

    // Set Save as PDF printer.
    await setSaveAsPdfDestination();

    // Save as PDF printer has media size capability.
    assertTrue(model.settings.mediaSize.available);

    // PDF to PDF -> media size is unavailable.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.mediaSize.available);
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: true});

    // Even if all pages have specified their orientation, the size option
    // should still be available.
    model.documentSettings = createDocumentSettings(
        model.documentSettings, {allPagesHaveCustomOrientation: true});
    await microtasksFinished();
    assertTrue(model.settings.mediaSize.available);
    model.documentSettings = createDocumentSettings(
        model.documentSettings, {allPagesHaveCustomOrientation: false});
    await microtasksFinished();

    // If all pages have specified a size, the size option shouldn't be
    // available.
    model.documentSettings = createDocumentSettings(
        model.documentSettings, {allPagesHaveCustomSize: true});
    await microtasksFinished();
    assertFalse(model.settings.mediaSize.available);
    assertFalse(model.settings.color.setFromUi);
  });

  test('margins', async function() {
    // The settings are available since isModifiable is true.
    assertTrue(model.settings.margins.available);
    assertTrue(model.settings.customMargins.available);

    // No margins settings for PDFs.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.margins.available);
    assertFalse(model.settings.customMargins.available);
    assertFalse(model.settings.margins.setFromUi);
    assertFalse(model.settings.customMargins.setFromUi);
  });

  test('dpi', function() {
    // The settings are available since the printer has multiple DPI options.
    assertTrue(model.settings.dpi.available);

    // Remove capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.dpi;

    // Section should now be hidden.
    simulateCapabilitiesChange(capabilities);
    assertFalse(model.settings.dpi.available);

    // Does not show up for only 1 option. Unavailable value should be set to
    // the only available option.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer.dpi!.option.pop();
    simulateCapabilitiesChange(capabilities);
    assertFalse(model.settings.dpi.available);
    assertEquals(200, model.settings.dpi.unavailableValue.horizontal_dpi);
    assertEquals(200, model.settings.dpi.unavailableValue.vertical_dpi);
    assertFalse(model.settings.dpi.setFromUi);
  });

  test('scaling', async function() {
    // HTML -> printer
    assertTrue(model.settings.scaling.available);

    // HTML -> Save as PDF
    const defaultDestination = model.destination;
    await setSaveAsPdfDestination();
    assertTrue(model.settings.scaling.available);

    // PDF -> Save as PDF
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.scaling.available);

    // PDF -> printer
    model.destination = defaultDestination;
    await microtasksFinished();
    assertTrue(model.settings.scaling.available);
    assertFalse(model.settings.scaling.setFromUi);
  });

  test('scalingType', async function() {
    // HTML -> printer
    assertTrue(model.settings.scalingType.available);

    // HTML -> Save as PDF
    const defaultDestination = model.destination;
    await setSaveAsPdfDestination();
    assertTrue(model.settings.scalingType.available);

    // PDF -> Save as PDF
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.scalingType.available);

    // PDF -> printer
    model.destination = defaultDestination;
    await microtasksFinished();
    assertFalse(model.settings.scalingType.available);
  });

  test('scalingTypePdf', async function() {
    // HTML -> printer
    assertFalse(model.settings.scalingTypePdf.available);

    // HTML -> Save as PDF
    const defaultDestination = model.destination;
    await setSaveAsPdfDestination();
    assertFalse(model.settings.scalingTypePdf.available);

    // PDF -> Save as PDF
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.scalingTypePdf.available);

    // PDF -> printer
    model.destination = defaultDestination;
    await microtasksFinished();
    assertTrue(model.settings.scalingTypePdf.available);
  });

  test('header footer', async function() {
    // Default margins + letter paper + HTML page.
    assertTrue(model.settings.headerFooter.available);

    // Custom margins initializes with customMargins undefined and margins
    // values matching the defaults.
    model.setSetting('margins', MarginsType.CUSTOM);
    assertTrue(model.settings.headerFooter.available);

    // Set margins to NONE
    model.setSetting('margins', MarginsType.NO_MARGINS);
    assertFalse(model.settings.headerFooter.available);

    // Set margins to MINIMUM
    model.setSetting('margins', MarginsType.MINIMUM);
    assertTrue(model.settings.headerFooter.available);

    // Custom margins of 0.
    model.setSetting('margins', MarginsType.CUSTOM);
    model.setSetting(
        'customMargins',
        {marginTop: 0, marginLeft: 0, marginRight: 0, marginBottom: 0});
    model.margins = new Margins(0, 0, 0, 0);
    await microtasksFinished();
    assertFalse(model.settings.headerFooter.available);

    // Custom margins of 36 -> header/footer available
    model.setSetting(
        'customMargins',
        {marginTop: 36, marginLeft: 36, marginRight: 36, marginBottom: 36});
    model.margins = new Margins(36, 36, 36, 36);
    await microtasksFinished();
    assertTrue(model.settings.headerFooter.available);

    // Zero top and bottom -> header/footer unavailable
    model.setSetting(
        'customMargins',
        {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 0});
    model.margins = new Margins(0, 36, 0, 36);
    await microtasksFinished();
    assertFalse(model.settings.headerFooter.available);

    // Zero top and nonzero bottom -> header/footer available
    model.setSetting(
        'customMargins',
        {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 36});
    model.margins = new Margins(0, 36, 36, 36);
    await microtasksFinished();
    assertTrue(model.settings.headerFooter.available);

    // Small paper sizes
    const capabilities = getCddTemplate(model.destination.id).capabilities!;
    capabilities.printer.media_size = {
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
    simulateCapabilitiesChange(capabilities);
    model.setSetting('margins', MarginsType.DEFAULT);

    // Header/footer should be available for default big label with
    // default margins.
    assertTrue(model.settings.headerFooter.available);

    model.setSetting('mediaSize', capabilities.printer.media_size.option[0]);

    // Header/footer should not be available for small label
    assertFalse(model.settings.headerFooter.available);

    // Reset to big label.
    model.setSetting('mediaSize', capabilities.printer.media_size.option[1]);
    assertTrue(model.settings.headerFooter.available);

    // Header/footer is never available for PDFs.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.headerFooter.available);
    assertFalse(model.settings.headerFooter.setFromUi);
  });

  test('css background', async function() {
    // The setting is available since isModifiable is true.
    assertTrue(model.settings.cssBackground.available);

    // No CSS background setting for PDFs.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.cssBackground.available);
    assertFalse(model.settings.cssBackground.setFromUi);
  });

  test('duplex', function() {
    assertTrue(model.settings.duplex.available);
    assertTrue(model.settings.duplexShortEdge.available);

    // Remove duplex capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.duplex;
    simulateCapabilitiesChange(capabilities);
    assertFalse(model.settings.duplex.available);
    assertFalse(model.settings.duplexShortEdge.available);

    // Set a duplex capability with only 1 type, no duplex.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.duplex;
    capabilities.printer.duplex = {
      option: [{type: DuplexType.NO_DUPLEX, is_default: true}],
    };
    simulateCapabilitiesChange(capabilities);
    assertFalse(model.settings.duplex.available);
    assertFalse(model.settings.duplexShortEdge.available);

    // Set a duplex capability with 2 types, long edge and no duplex.
    capabilities = getCddTemplate(model.destination.id).capabilities!;
    delete capabilities.printer.duplex;
    capabilities.printer.duplex = {
      option: [
        {type: DuplexType.NO_DUPLEX},
        {type: DuplexType.LONG_EDGE, is_default: true},
      ] as DuplexOption[],
    };
    simulateCapabilitiesChange(capabilities);
    assertTrue(model.settings.duplex.available);
    assertFalse(model.settings.duplexShortEdge.available);
    assertFalse(model.settings.duplex.setFromUi);
    assertFalse(model.settings.duplexShortEdge.setFromUi);
  });

  // <if expr="is_linux">
  test('rasterize', async function() {
    // Availability for PDFs varies depening upon OS.
    // Windows and macOS depend on policy - see policy_test.js for their
    // testing coverage.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    // Always available for PDFs on Linux.
    assertTrue(model.settings.rasterize.available);
    assertFalse(model.settings.rasterize.setFromUi);
  });
  // </if>

  test('selection only', async function() {
    // Not available with no selection.
    assertFalse(model.settings.selectionOnly.available);

    model.documentSettings =
        createDocumentSettings(model.documentSettings, {hasSelection: true});
    await microtasksFinished();
    assertTrue(model.settings.selectionOnly.available);

    // Not available for PDFs.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertFalse(model.settings.selectionOnly.available);
    assertFalse(model.settings.selectionOnly.setFromUi);
  });

  test('pages per sheet', async function() {
    // Pages per sheet is available everywhere except for ARC.
    // With the default settings for Blink content, it is available.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: true});
    await microtasksFinished();
    assertTrue(model.settings.pagesPerSheet.available);

    // Still available for PDF content.
    model.documentSettings =
        createDocumentSettings(model.documentSettings, {isModifiable: false});
    await microtasksFinished();
    assertTrue(model.settings.pagesPerSheet.available);
  });
});
