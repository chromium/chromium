// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ColorOption, DuplexOption, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {IPP_PRINT_QUALITY, ManagedPrintOptionsDuplexType, ManagedPrintOptionsQualityType/*, Size*/} from 'chrome://print/print_preview.js';
import type {DestinationOptionalParams, ManagedPrintOptions} from 'chrome://print/print_preview.js';

import {
  getCddTemplate,
  getCddTemplateWithAdvancedSettings} from './print_preview_test_utils.js';

suite('ManagedPrintOptionsTest', () => {
  let model: PrintPreviewModelElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
  });

  function initializeModel() {
    model.documentSettings = {
      allPagesHaveCustomSize: false,
      allPagesHaveCustomOrientation: false,
      hasSelection: true,
      isModifiable: true,
      isScalingDisabled: false,
      fitToPageScaling: 100,
      pageCount: 3,
      isFromArc: false,
      title: 'title',
    };
    // Set rasterize available so that it can be tested.
    model.set('settings.rasterize.available', true);
  }

  test('DisabledViaExperiment', () => {
    loadTimeData.overrideValues(
        {isUseManagedPrintJobOptionsInPrintPreviewEnabled: false});
    const managedPrintOptions: ManagedPrintOptions = {
      color: {defaultValue: false},
      mediaType: {allowedValues: ['photographic']},
    };
    const params: DestinationOptionalParams = {
      managedPrintOptions: managedPrintOptions,
    };
    const testDestination1 = new Destination(
        /*id_=*/ 'TestDestination1',
        /*origin_=*/ DestinationOrigin.LOCAL,
        /*displayName_=*/ 'TestDestination1',
        /*params_=*/ params);
    testDestination1.capabilities =
        getCddTemplate('TestDestination1').capabilities;
    initializeModel();
    model.destination = testDestination1;

    model.applyDestinationSpecificPolicies();

    // Managed per-printers options are disabled via experiment, so the
    // destination default and allowed values should be used.
    assertTrue(model.getSettingValue('color'));
    assertEquals(
        testDestination1.capabilities!.printer.media_type!.option.length, 2);
  });

  test('SupportedDefaultValues', () => {
    loadTimeData.overrideValues(
        {isUseManagedPrintJobOptionsInPrintPreviewEnabled: true});
    const managedPrintOptions: ManagedPrintOptions = {
      mediaSize: {defaultValue: {width: 101600, height: 152400}},
      mediaType: {defaultValue: 'photographic'},
      duplex: {defaultValue: ManagedPrintOptionsDuplexType.SHORT_EDGE},
      color: {defaultValue: false},
      dpi: {defaultValue: {horizontal: 100, vertical: 100}},
      quality: {defaultValue: ManagedPrintOptionsQualityType.DRAFT},
      printAsImage: {defaultValue: true},
    };
    const params: DestinationOptionalParams = {
      managedPrintOptions: managedPrintOptions,
    };
    const testDestination1 = new Destination(
        /*id_=*/ 'TestDestination1',
        /*origin_=*/ DestinationOrigin.LOCAL,
        /*displayName_=*/ 'TestDestination1',
        /*params_=*/ params);
    testDestination1.capabilities =
        getCddTemplateWithAdvancedSettings(5, 'TestDestination1').capabilities;
    initializeModel();

    model.destination = testDestination1;
    model.applyDestinationSpecificPolicies();

    assertEquals('4x6', model.getSettingValue('mediaSize').name);
    assertEquals('photographic', model.getSettingValue('mediaType').vendor_id);
    assertTrue(model.getSettingValue('duplex'));
    assertTrue(model.getSettingValue('duplexShortEdge'));
    assertFalse(model.getSettingValue('color'));
    assertEquals(100, model.getSettingValue('dpi').horizontal_dpi);
    assertEquals(100, model.getSettingValue('dpi').vertical_dpi);
    assertEquals(
        /* ManagedPrintOptionsQualityType.DRAFT */ '3',
        model.getSettingValue('vendorItems')[IPP_PRINT_QUALITY]);
    assertTrue(model.getSettingValue('rasterize'));
  });

  test('UnsupportedDefaultValues', () => {
    loadTimeData.overrideValues(
        {isUseManagedPrintJobOptionsInPrintPreviewEnabled: true});
    const managedPrintOptions: ManagedPrintOptions = {
      mediaSize: {defaultValue: {width: 1, height: 1}},
      mediaType: {defaultValue: 'abacaba'},
      duplex: {defaultValue: ManagedPrintOptionsDuplexType.UNKNOWN_DUPLEX},
      color: {defaultValue: true},
      dpi: {defaultValue: {horizontal: 1, vertical: 1}},
      quality: {defaultValue: ManagedPrintOptionsQualityType.UNKNOWN_QUALITY},
      printAsImage: {defaultValue: true},
    };
    const params: DestinationOptionalParams = {
      managedPrintOptions: managedPrintOptions,
    };
    const testDestination1 = new Destination(
        /*id_=*/ 'TestDestination1',
        /*origin_=*/ DestinationOrigin.LOCAL,
        /*displayName_=*/ 'TestDestination1',
        /*params_=*/ params);
    testDestination1.capabilities =
        getCddTemplateWithAdvancedSettings(5, 'TestDestination1').capabilities;
    testDestination1.capabilities!.printer.color = {
      option: [
        {type: 'STANDARD_MONOCHROME', is_default: true},
      ] as ColorOption[],
    };
    initializeModel();
    model.set('settings.rasterize.available', false);
    // Now all the default values in `managedPrintOptions` are unsupported.

    model.destination = testDestination1;
    model.applyDestinationSpecificPolicies();

    // For each setting the destination default value should be selected.
    assertEquals('NA_LETTER', model.getSettingValue('mediaSize').name);
    assertEquals('stationery', model.getSettingValue('mediaType').vendor_id);
    assertFalse(model.getSettingValue('duplex'));
    assertFalse(model.getSettingValue('duplexShortEdge'));
    assertFalse(model.getSettingValue('color'));
    assertEquals(200, model.getSettingValue('dpi').horizontal_dpi);
    assertEquals(200, model.getSettingValue('dpi').vertical_dpi);
    assertEquals(
        /* ManagedPrintOptionsQualityType.NORMAL */ '4',
        model.getSettingValue('vendorItems')[IPP_PRINT_QUALITY]);
    assertFalse(model.getSettingValue('rasterize'));
  });

  test('AllowedValuesApplied', () => {
    loadTimeData.overrideValues(
        {isUseManagedPrintJobOptionsInPrintPreviewEnabled: true});
    const managedPrintOptions: ManagedPrintOptions = {
      mediaSize: {
        // {width: 123, height: 321} is not supported by this printer.
        allowedValues: [
          {width: 215900, height: 279400},
          {width: 215900, height: 215900},
          {width: 123, height: 321},
        ],
      },
      // 'unsupported_value' is not supported by this printer.
      mediaType: {allowedValues: ['photographic', 'unsupported_value']},
      duplex: {
        allowedValues: [
          ManagedPrintOptionsDuplexType.ONE_SIDED,
          ManagedPrintOptionsDuplexType.SHORT_EDGE,
        ],
      },
      color: {allowedValues: [false]},
      // {horizontal: 123, vertical: 321} is not supported by this printer.
      dpi: {
        allowedValues: [
          {horizontal: 100, vertical: 100},
          {horizontal: 123, vertical: 321},
        ],
      },
      quality: {
        allowedValues: [
          ManagedPrintOptionsQualityType.DRAFT,
        ],
      },
    };
    const params: DestinationOptionalParams = {
      managedPrintOptions: managedPrintOptions,
    };
    const testDestination1 = new Destination(
        /*id_=*/ 'TestDestination1',
        /*origin_=*/ DestinationOrigin.LOCAL,
        /*displayName_=*/ 'TestDestination1',
        /*params_=*/ params);

    testDestination1.capabilities =
        getCddTemplateWithAdvancedSettings(5, 'TestDestination1').capabilities;

    // Allowed values are the intersection of values supported by printer and of
    // allowed values set via managed print options.
    const allowedCapabilities = testDestination1.capabilities;
    assertEquals(2, allowedCapabilities!.printer.media_size!.option.length);
    assertEquals(1, allowedCapabilities!.printer.media_type!.option.length);
    assertEquals(2, allowedCapabilities!.printer.duplex!.option.length);
    assertEquals(1, allowedCapabilities!.printer.color!.option.length);
    assertEquals(1, allowedCapabilities!.printer.dpi!.option.length);
    const vendorCapabilities = allowedCapabilities!.printer.vendor_capability;
    const printQualityCapabilities = vendorCapabilities!.find(o => {
      return o.id === IPP_PRINT_QUALITY;
    });
    assertEquals(1, printQualityCapabilities!.select_cap!.option!.length);
  });

  test('AllowedValuesIgnored', () => {
    loadTimeData.overrideValues(
        {isUseManagedPrintJobOptionsInPrintPreviewEnabled: true});
    const managedPrintOptions: ManagedPrintOptions = {
      mediaSize: {
        allowedValues: [{width: 123, height: 321}],
      },
      mediaType: {allowedValues: ['unsupported_value']},
      duplex: {
        allowedValues: [ManagedPrintOptionsDuplexType.ONE_SIDED],
      },
      color: {allowedValues: [true]},
      dpi: {allowedValues: [{horizontal: 123, vertical: 321}]},
      quality: {
        allowedValues: [
          ManagedPrintOptionsQualityType.HIGH,
        ],
      },
    };
    const params: DestinationOptionalParams = {
      managedPrintOptions: managedPrintOptions,
    };
    const testDestination1 = new Destination(
        /*id_=*/ 'TestDestination1',
        /*origin_=*/ DestinationOrigin.LOCAL,
        /*displayName_=*/ 'TestDestination1',
        /*params_=*/ params);
    const capabilities =
        getCddTemplateWithAdvancedSettings(5, 'TestDestination1').capabilities;
    capabilities!.printer.duplex = {
      option: [
        {type: 'LONG_EDGE', is_default: true},
        {type: 'SHORT_EDGE'},
      ] as DuplexOption[],
    };
    capabilities!.printer.color = {
      option: [
        {type: 'STANDARD_MONOCHROME', is_default: true},
      ] as ColorOption[],
    };
    testDestination1.capabilities = capabilities;
    // All the allowed values in 'managedPrintOptions' are unsupported by the
    // printer. Thus they are ignored.

    // Allowed values are exactly the values this printer supports.
    const allowedCapabilities = testDestination1.capabilities;
    assertEquals(4, allowedCapabilities!.printer.media_size!.option.length);
    assertEquals(2, allowedCapabilities!.printer.media_type!.option.length);
    assertEquals(2, allowedCapabilities!.printer.duplex!.option.length);
    assertEquals(1, allowedCapabilities!.printer.color!.option.length);
    assertEquals(2, allowedCapabilities!.printer.dpi!.option.length);
    const vendorCapabilities = allowedCapabilities!.printer.vendor_capability;
    const printQualityCapabilities = vendorCapabilities!.find(o => {
      return o.id === IPP_PRINT_QUALITY;
    });
    assertEquals(2, printQualityCapabilities!.select_cap!.option!.length);
  });
});
