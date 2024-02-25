// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {ColorModeRestriction, Destination, DestinationOrigin, DuplexModeRestriction, Margins,
        // <if expr="is_chromeos">
        PinModeRestriction,
        // </if>
        Size} from 'chrome://print/print_preview.js';
// <if expr="is_chromeos">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {getCddTemplate} from './print_preview_test_utils.js';

suite('ModelSettingsPolicyTest', function() {
  let model: PrintPreviewModelElement;

  function setupModel() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    model.documentSettings = {
      allPagesHaveCustomSize: false,
      allPagesHaveCustomOrientation: false,
      hasSelection: false,
      isModifiable: true,
      isScalingDisabled: false,
      fitToPageScaling: 100,
      pageCount: 3,
      isFromArc: false,
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
  }

  test('color managed', function() {
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
           {type: 'STANDARD_COLOR'},
         ],
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
           {type: 'STANDARD_COLOR'},
         ],
       },
       colorDefault: ColorModeRestriction.COLOR,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     },
     {
       // Default defined by policy but setting is modifiable (same as the case
       // above but with swapped defaults).
       colorCap: {
         option: [
           {type: 'STANDARD_MONOCHROME'},
           {type: 'STANDARD_COLOR', is_default: true},
         ],
       },
       colorDefault: ColorModeRestriction.MONOCHROME,
       expectedValue: false,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
     }].forEach(subtestParams => {
      setupModel();
      // Remove color capability.
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer!.color = subtestParams.colorCap;
      const policies = {
        color: {
          allowedMode: subtestParams.colorPolicy,
          defaultMode: subtestParams.colorDefault,
        },
      };

      model.set('destination.capabilities', capabilities);
      model.setPolicySettings(policies);
      model.applyStickySettings();

      assertEquals(subtestParams.expectedValue, model.getSettingValue('color'));
      assertEquals(
          subtestParams.expectedAvailable, model.settings.color.available);
      assertEquals(subtestParams.expectedManaged, model.settingsManaged);
      assertEquals(
          subtestParams.expectedEnforced, model.settings.color.setByPolicy);
    });
  });

  test('duplex managed', function() {
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
           {type: 'NO_DUPLEX', is_default: true},
           {type: 'LONG_EDGE'},
           {type: 'SHORT_EDGE'},
         ],
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
       // Policies are unset.
       duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
       duplexPolicy: DuplexModeRestriction.UNSET,
       duplexDefault: DuplexModeRestriction.UNSET,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: false,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: false,
       expectedShortEdgeEnforced: false,
     },
     {
       // Policies are undefined.
       duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
       duplexPolicy: undefined,
       duplexDefault: undefined,
       expectedValue: false,
       expectedAvailable: false,
       expectedManaged: false,
       expectedEnforced: false,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: false,
       expectedShortEdgeEnforced: false,
     },
     // Couple of tests that verify the default and available duplex values set
     // by policies.
     // Default printing destination duplex mode should always be overwritten by
     // the policy default.
     {
       duplexCap: {
         option: [
           {type: 'NO_DUPLEX', is_default: true},
           {type: 'LONG_EDGE'},
           {type: 'SHORT_EDGE'},
         ],
       },
       duplexPolicy: DuplexModeRestriction.DUPLEX,
       duplexDefault: DuplexModeRestriction.SHORT_EDGE,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
       expectedShortEdge: true,
       expectedShortEdgeAvailable: true,
       expectedShortEdgeEnforced: false,
     },
     {
       duplexCap: {
         option: [
           {type: 'NO_DUPLEX'},
           {type: 'LONG_EDGE'},
           {type: 'SHORT_EDGE', is_default: true},
         ],
       },
       duplexPolicy: DuplexModeRestriction.UNSET,
       duplexDefault: DuplexModeRestriction.LONG_EDGE,
       expectedValue: true,
       expectedAvailable: true,
       expectedManaged: false,
       expectedEnforced: false,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: true,
       expectedShortEdgeEnforced: false,
     },
     {
       duplexCap: {
         option: [
           {type: 'NO_DUPLEX'},
           {type: 'LONG_EDGE', is_default: true},
           {type: 'SHORT_EDGE'},
         ],
       },
       duplexPolicy: DuplexModeRestriction.SIMPLEX,
       duplexDefault: DuplexModeRestriction.SIMPLEX,
       expectedValue: false,
       expectedAvailable: true,
       expectedManaged: true,
       expectedEnforced: true,
       expectedShortEdge: false,
       expectedShortEdgeAvailable: true,
       expectedShortEdgeEnforced: false,
     }].forEach(subtestParams => {
      setupModel();
      // Remove duplex capability.
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer!.duplex = subtestParams.duplexCap;
      const policies = {
        duplex: {
          allowedMode: subtestParams.duplexPolicy,
          defaultMode: subtestParams.duplexDefault,
        },
      };

      model.set('destination.capabilities', capabilities);
      model.setPolicySettings(policies);
      model.applyStickySettings();

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

  // <if expr="is_chromeos">
  test('pin managed', function() {
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
      setupModel();
      // Make device enterprise managed since pin setting is available only on
      // managed devices.
      loadTimeData.overrideValues({isEnterpriseManaged: true});
      // Remove pin capability.
      const capabilities = getCddTemplate(model.destination.id).capabilities!;
      capabilities.printer!.pin = subtestParams.pinCap;
      const policies = {
        pin: {
          allowedMode: subtestParams.pinPolicy,
          defaultMode: subtestParams.pinDefault,
        },
      };

      model.set('destination.capabilities', capabilities);
      model.setPolicySettings(policies);
      model.applyStickySettings();

      assertEquals(subtestParams.expectedValue, model.getSettingValue('pin'));
      assertEquals(
          subtestParams.expectedAvailable, model.settings.pin.available);
      assertEquals(subtestParams.expectedManaged, model.settingsManaged);
      assertEquals(
          subtestParams.expectedEnforced, model.settings.pin.setByPolicy);
    });
  });
  // </if>
});
