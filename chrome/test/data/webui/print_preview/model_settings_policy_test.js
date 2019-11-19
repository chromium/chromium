// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundGraphicsModeRestriction, ColorModeRestriction, Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, DuplexModeRestriction, Margins, MarginsType, PinModeRestriction, Size} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getCddTemplate} from 'chrome://test/print_preview/print_preview_test_utils.js';

suite('ModelSettingsPolicyTest', function() {
  let model = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    model.documentSettings = {
      hasCssMediaStyles: false,
      hasSelection: false,
      isModifiable: true,
      isScalingDisabled: false,
      fitToPageScaling: 100,
      pageCount: 3,
      title: 'title',
    };

    model.pageSize = new Size(612, 792);
    model.margins = new Margins(72, 72, 72, 72);

    // Create a test destination.
    model.destination = new Destination(
        'FooDevice', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'FooName',
        DestinationConnectionStatus.ONLINE);
    model.set(
        'destination.capabilities',
        getCddTemplate(model.destination.id).capabilities);
  });

  test('color managed', function() {
    // Remove color capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities;
    delete capabilities.printer.color;

    [{
      // Policy has no effect, setting unavailable
      colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
      colorPolicy: ColorModeRestriction.COLOR,
      colorDefault: ColorModeRestriction.COLOR,
      expectedValue: true,
      expectedAvailable: false,
      expectedManaged: false,
      expectedEnforced: true,
    },
     {
       // Policy contradicts actual capabilities, setting unavailable.
       colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
       colorPolicy: ColorModeRestriction.MONOCHROME,
       colorDefault: ColorModeRestriction.MONOCHROME,
       expectedValue: true,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: true,
     },
     {
       // Policy overrides default.
       colorCap: {
         option: [
           {type: 'STANDARD_MONOCHROME', is_default: true},
           {type: 'STANDARD_COLOR'}
         ]
       },
       colorPolicy: ColorModeRestriction.COLOR,
       // Default mismatches restriction and is ignored.
       colorDefault: ColorModeRestriction.MONOCHROME,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
     },
     {
       // Default defined by policy but setting is modifiable.
       colorCap: {
         option: [
           {type: 'STANDARD_MONOCHROME', is_default: true},
           {type: 'STANDARD_COLOR'}
         ]
       },
       colorDefault: ColorModeRestriction.COLOR,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     }].forEach(subtestParams => {
      capabilities = getCddTemplate(model.destination.id).capabilities;
      capabilities.printer.color = subtestParams.colorCap;
      const policies = {
        allowedColorModes: subtestParams.colorPolicy,
        defaultColorMode: subtestParams.colorDefault,
      };
      // In practice |capabilities| are always set after |policies| and
      // observers only check for |capabilities|, so the order is important.
      model.set('destination.policies', policies);
      model.set('destination.capabilities', capabilities);
      model.applyDestinationSpecificPolicies();
      assertEquals(subtestParams.expectedValue, model.getSettingValue('color'));
      assertEquals(
          subtestParams.expectedAvailable, model.settings.color.available);
      assertEquals(subtestParams.expectedManaged, model.settingsManaged);
      assertEquals(
          subtestParams.expectedEnforced, model.settings.color.setByPolicy);
    });
  });

  test('duplex managed', function() {
    // Remove duplex capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities;
    delete capabilities.printer.duplex;

    [{
      // Policy has no effect.
      duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
      duplexPolicy: DuplexModeRestriction.SIMPLEX,
      duplexDefault: DuplexModeRestriction.SIMPLEX,
      expectedValue: false,
      expectedAvailable: false,
      expectedManaged: false,
      expectedEnforced: true,
      expectedShortEdge: false,
      expectedShortEdgeAvailable: false,
      expectedShortEdgeEnforced: false,
    },
     {
       // Policy contradicts actual capabilities and is ignored.
       duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
       duplexPolicy: DuplexModeRestriction.DUPLEX,
       duplexDefault: DuplexModeRestriction.LONG_EDGE,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: true,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: false,
       expectedShortEdgeEnforced: false,
     },
     {
       // Policy overrides default.
       duplexCap: {
         option: [
           {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
           {type: 'SHORT_EDGE'}
         ]
       },
       duplexPolicy: DuplexModeRestriction.DUPLEX,
       // Default mismatches restriction and is ignored.
       duplexDefault: DuplexModeRestriction.SIMPLEX,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: true,
       expectedShortEdgeEnforced: false,
     },
     {
       // Policy sets duplex type, overriding default.
       duplexCap: {
         option: [
           {type: 'NO_DUPLEX'}, {type: 'LONG_EDGE', is_default: true},
           {type: 'SHORT_EDGE'}
         ]
       },
       duplexPolicy: DuplexModeRestriction.SHORT_EDGE,
       // Default mismatches restriction and is ignored.
       duplexDefault: DuplexModeRestriction.LONG_EDGE,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
       expectedShortEdge: true,
       expectedShortEdgeAvailable: true,
       expectedShortEdgeEnforced: true,
     },
     {
       // Default defined by policy but setting is modifiable.
       duplexCap: {
         option: [
           {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
           {type: 'SHORT_EDGE'}
         ]
       },
       duplexDefault: DuplexModeRestriction.LONG_EDGE,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: true,
       expectedShortEdgeEnforced: false,
     }].forEach(subtestParams => {
      capabilities = getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.duplex = subtestParams.duplexCap;
      const policies = {
        allowedDuplexModes: subtestParams.duplexPolicy,
        defaultDuplexMode: subtestParams.duplexDefault,
      };
      // In practice |capabilities| are always set after |policies| and
      // observers only check for |capabilities|, so the order is important.
      model.set('destination.policies', policies);
      model.set('destination.capabilities', capabilities);
      model.applyDestinationSpecificPolicies();
      assertEquals(
          subtestParams.expectedValue, model.getSettingValue('duplex'));
      assertEquals(
          subtestParams.expectedAvailable, model.settings.duplex.available);
      assertEquals(subtestParams.expectedManaged, model.settingsManaged);
      assertEquals(
          subtestParams.expectedEnforced, model.settings.duplex.setByPolicy);
      assertEquals(
          subtestParams.expectedShortEdge,
          model.getSettingValue('duplexShortEdge'));
      assertEquals(
          subtestParams.expectedShortEdgeAvailable,
          model.settings.duplexShortEdge.available);
      assertEquals(
          subtestParams.expectedShortEdgeEnforced,
          model.settings.duplexShortEdge.setByPolicy);
    });
  });

  test('pin managed', function() {
    // Remove pin capability.
    let capabilities = getCddTemplate(model.destination.id).capabilities;
    delete capabilities.printer.pin;

    // Make device enterprise managed since pin setting is available only on
    // managed devices.
    loadTimeData.overrideValues({isEnterpriseManaged: true});

    [{
      // No policies, settings is modifiable.
      pinCap: {supported: true},
      expectedValue: false,
      expectedAvailable: true,
      expectedManaged: false,
      expectedEnforced: false,
    },
     {
       // Policy has no effect, setting unavailable.
       pinCap: {},
       pinPolicy: PinModeRestriction.PIN,
       pinDefault: PinModeRestriction.PIN,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: true,
     },
     {
       // Policy has no effect, setting is not supported.
       pinCap: {supported: false},
       pinPolicy: PinModeRestriction.UNSET,
       pinDefault: PinModeRestriction.PIN,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: false,
     },
     {
       // Policy is UNSECURE, setting is not available.
       pinCap: {supported: true},
       pinPolicy: PinModeRestriction.NO_PIN,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: true,
     },
     {
       // No restriction policy, setting is modifiable.
       pinCap: {supported: true},
       pinPolicy: PinModeRestriction.UNSET,
       pinDefault: PinModeRestriction.NO_PIN,
       expectedValue: false,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     },
     {
       // Policy overrides default.
       pinCap: {supported: true},
       pinPolicy: PinModeRestriction.PIN,
       // Default mismatches restriction and is ignored.
       pinDefault: PinModeRestriction.NO_PIN,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
     },
     {
       // Default defined by policy but setting is modifiable.
       pinCap: {supported: true},
       pinDefault: PinModeRestriction.PIN,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     }].forEach(subtestParams => {
      capabilities = getCddTemplate(model.destination.id).capabilities;
      capabilities.printer.pin = subtestParams.pinCap;
      const policies = {
        allowedPinModes: subtestParams.pinPolicy,
        defaultPinMode: subtestParams.pinDefault,
      };
      // In practice |capabilities| are always set after |policies| and
      // observers only check for |capabilities|, so the order is important.
      model.set('destination.policies', policies);
      model.set('destination.capabilities', capabilities);
      model.applyDestinationSpecificPolicies();
      assertEquals(subtestParams.expectedValue, model.getSettingValue('pin'));
      assertEquals(
          subtestParams.expectedAvailable, model.settings.pin.available);
      assertEquals(subtestParams.expectedManaged, model.settingsManaged);
      assertEquals(
          subtestParams.expectedEnforced, model.settings.pin.setByPolicy);
    });
  });

  test('background graphics managed', function() {
    [{
      // No policies, settings is modifiable.
      documentIsModifiable: true,
      expectedValue: false,
      expectedAvailable: true,
      expectedManaged: false,
      expectedEnforced: false,
    },
     {
       // Policy has no effect, setting is not available for non-modifiable
       // content.
       documentIsModifiable: false,
       backgroundGraphicsPolicy: BackgroundGraphicsModeRestriction.UNSET,
       backgroundGraphicsDefault: BackgroundGraphicsModeRestriction.ENABLED,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: false,
     },
     {
       // Policy is DISABLED, setting is not modifiable.
       documentIsModifiable: true,
       backgroundGraphicsPolicy: BackgroundGraphicsModeRestriction.DISABLED,
       expectedValue: false,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
     },
     {
       // No restriction policy, setting is modifiable.
       documentIsModifiable: true,
       backgroundGraphicsPolicy: BackgroundGraphicsModeRestriction.UNSET,
       backgroundGraphicsDefault: BackgroundGraphicsModeRestriction.DISABLED,
       expectedValue: false,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     },
     {
       // Policy overrides default.
       documentIsModifiable: true,
       backgroundGraphicsPolicy: BackgroundGraphicsModeRestriction.ENABLED,
       // Default mismatches restriction and is ignored.
       backgroundGraphicsDefault: BackgroundGraphicsModeRestriction.DISABLED,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
     },
     {
       // Default defined by policy but setting is modifiable.
       documentIsModifiable: true,
       backgroundGraphicsDefault: BackgroundGraphicsModeRestriction.ENABLED,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     }].forEach(subtestParams => {
      model.set(
          'documentSettings.isModifiable', subtestParams.documentIsModifiable);
      const policies = {
        allowedBackgroundGraphicsModes: subtestParams.backgroundGraphicsPolicy,
        defaultBackgroundGraphicsMode: subtestParams.backgroundGraphicsDefault,
      };
      model.set('destination.policies', policies);
      model.applyDestinationSpecificPolicies();
      assertEquals(
          subtestParams.expectedValue, model.getSettingValue('cssBackground'));
      assertEquals(
          subtestParams.expectedAvailable,
          model.settings.cssBackground.available);
      assertEquals(subtestParams.expectedManaged, model.settingsManaged);
      assertEquals(
          subtestParams.expectedEnforced,
          model.settings.cssBackground.setByPolicy);
    });
  });
});
