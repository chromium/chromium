// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NativeInitialSettings, PrintPreviewAppElement, SerializedSettings, Settings, SettingsMixinInterface} from 'chrome://print/print_preview.js';
import {getInstance, MarginsType, NativeLayerImpl, PluginProxyImpl, ScalingType} from 'chrome://print/print_preview.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplateWithAdvancedSettings, getDefaultInitialSettings} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('RestoreStateTest', function() {
  let page: PrintPreviewAppElement;
  let nativeLayer: NativeLayerStub;

  const initialSettings: NativeInitialSettings = getDefaultInitialSettings();

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * @param stickySettings Settings to verify.
   */
  function verifyStickySettingsApplied(stickySettings: SerializedSettings) {
    assertEquals(
        stickySettings.dpi!.horizontal_dpi,
        page.settings.dpi.value.horizontal_dpi);
    assertEquals(
        stickySettings.dpi!.vertical_dpi, page.settings.dpi.value.vertical_dpi);
    assertEquals(
        stickySettings.mediaSize!.name, page.settings.mediaSize.value.name);
    assertEquals(
        stickySettings.mediaSize!.height_microns,
        page.settings.mediaSize.value.height_microns);
    assertEquals(
        stickySettings.mediaSize!.width_microns,
        page.settings.mediaSize.value.width_microns);
    assertEquals(
        (stickySettings.vendorOptions! as {[key: string]: any})!['paperType'],
        page.settings.vendorItems.value.paperType);
    assertEquals(
        (stickySettings.vendorOptions! as {[key: string]: any})['printArea'],
        page.settings.vendorItems.value.printArea);

    ([
      ['margins', 'marginsType'],
      ['color', 'isColorEnabled'],
      ['headerFooter', 'isHeaderFooterEnabled'],
      ['layout', 'isLandscapeEnabled'],
      ['collate', 'isCollateEnabled'],
      ['cssBackground', 'isCssBackgroundEnabled'],
      ['scaling', 'scaling'],
      ['scalingType', 'scalingType'],
      ['scalingTypePdf', 'scalingTypePdf'],
    ] as Array<[keyof Settings, string]>)
        .forEach(keys => {
          assertEquals(
              (stickySettings as {[key: string]: any})[keys[1]!],
              page.settings![keys[0]!]!.value);
        });
  }

  /**
   * Performs initialization and verifies settings.
   */
  async function testInitializeWithStickySettings(
      stickySettings: SerializedSettings) {
    initialSettings.serializedAppStateStr = JSON.stringify(stickySettings);

    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplateWithAdvancedSettings(2, initialSettings.printerName));
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);

    await Promise.all([
      nativeLayer.whenCalled('getInitialSettings'),
      nativeLayer.whenCalled('getPrinterCapabilities'),
    ]);
    verifyStickySettingsApplied(stickySettings);
  }

  /**
   * Tests state restoration with all boolean settings set to true, scaling =
   * 90, dpi = 100, custom square paper, and custom margins.
   */
  test(
      'RestoreTrueValues', async function() {
        const stickySettings: SerializedSettings = {
          version: 2,
          recentDestinations: [],
          dpi: {horizontal_dpi: 100, vertical_dpi: 100},
          mediaSize: {
            name: 'CUSTOM',
            width_microns: 215900,
            height_microns: 215900,
            custom_display_name: 'CUSTOM_SQUARE',
            has_borderless_variant: true,
          },
          customMargins: {
            marginTop: 74,
            marginRight: 74,
            marginBottom: 74,
            marginLeft: 74,
          },
          vendorOptions: {
            paperType: 1,
            printArea: 6,
          },
          marginsType: 3, /* custom */
          scaling: '90',
          scalingType: ScalingType.CUSTOM,
          scalingTypePdf: ScalingType.FIT_TO_PAGE,
          isHeaderFooterEnabled: true,
          isCssBackgroundEnabled: true,
          isCollateEnabled: true,
          isDuplexEnabled: true,
          isDuplexShortEdge: true,
          isLandscapeEnabled: true,
          isColorEnabled: true,
          // <if expr="is_chromeos">
          isPinEnabled: true,
          pinValue: '0000',
          // </if>
        };
        await testInitializeWithStickySettings(stickySettings);
      });

  /**
   * Tests state restoration with all boolean settings set to false, scaling =
   * 120, dpi = 200, letter paper and default margins.
   */
  test(
      'RestoreFalseValues', async function() {
        const stickySettings: SerializedSettings = {
          version: 2,
          recentDestinations: [],
          dpi: {horizontal_dpi: 200, vertical_dpi: 200},
          mediaSize: {
            name: 'NA_LETTER',
            width_microns: 215900,
            height_microns: 279400,
            is_default: true,
            custom_display_name: 'Letter',
          },
          vendorOptions: {
            paperType: 0,
            printArea: 4,
          },
          marginsType: 0, /* default */
          scaling: '120',
          scalingType: ScalingType.DEFAULT,
          scalingTypePdf: ScalingType.DEFAULT,
          isHeaderFooterEnabled: false,
          isCssBackgroundEnabled: false,
          isCollateEnabled: false,
          isDuplexEnabled: false,
          isDuplexShortEdge: false,
          isLandscapeEnabled: false,
          isColorEnabled: false,
          // <if expr="is_chromeos">
          isPinEnabled: false,
          pinValue: '',
          // </if>
        };
        await testInitializeWithStickySettings(stickySettings);
      });

  /**
   * Tests that setting the settings values results in the correct serialized
   * values being sent to the native layer.
   */
  test('SaveValues', async function() {
    interface TestCase {
      section: string;
      settingName: keyof Settings;
      key: string;
      value: any;
    }

    /**
     * Array of section names, setting names, keys for serialized state, and
     * values for testing.
     */
    const testData: TestCase[] = [
      {
        section: 'print-preview-copies-settings',
        settingName: 'collate',
        key: 'isCollateEnabled',
        value: true,
      },
      {
        section: 'print-preview-layout-settings',
        settingName: 'layout',
        key: 'isLandscapeEnabled',
        value: true,
      },
      {
        section: 'print-preview-color-settings',
        settingName: 'color',
        key: 'isColorEnabled',
        value: false,
      },
      {
        section: 'print-preview-media-size-settings',
        settingName: 'mediaSize',
        key: 'mediaSize',
        value: {
          name: 'CUSTOM',
          width_microns: 215900,
          height_microns: 215900,
          custom_display_name: 'CUSTOM_SQUARE',
          has_borderless_variant: true,
        },
      },
      {
        section: 'print-preview-margins-settings',
        settingName: 'margins',
        key: 'marginsType',
        value: MarginsType.MINIMUM,
      },
      {
        section: 'print-preview-dpi-settings',
        settingName: 'dpi',
        key: 'dpi',
        value: {horizontal_dpi: 100, vertical_dpi: 100},
      },
      {
        section: 'print-preview-scaling-settings',
        settingName: 'scalingType',
        key: 'scalingType',
        value: ScalingType.CUSTOM,
      },
      {
        section: 'print-preview-scaling-settings',
        settingName: 'scalingTypePdf',
        key: 'scalingTypePdf',
        value: ScalingType.CUSTOM,
      },
      {
        section: 'print-preview-scaling-settings',
        settingName: 'scaling',
        key: 'scaling',
        value: '85',
      },
      {
        section: 'print-preview-duplex-settings',
        settingName: 'duplex',
        key: 'isDuplexEnabled',
        value: false,
      },
      {
        section: 'print-preview-duplex-settings',
        settingName: 'duplexShortEdge',
        key: 'isDuplexShortEdge',
        value: true,
      },
      {
        section: 'print-preview-other-options-settings',
        settingName: 'headerFooter',
        key: 'isHeaderFooterEnabled',
        value: false,
      },
      {
        section: 'print-preview-other-options-settings',
        settingName: 'cssBackground',
        key: 'isCssBackgroundEnabled',
        value: true,
      },
      {
        section: 'print-preview-advanced-options-settings',
        settingName: 'vendorItems',
        key: 'vendorOptions',
        value: {
          paperType: 1,
          printArea: 6,
        },
      },
      // <if expr="is_chromeos">
      {
        section: 'print-preview-pin-settings',
        settingName: 'pin',
        key: 'isPinEnabled',
        value: true,
      },
      {
        section: 'print-preview-pin-settings',
        settingName: 'pinValue',
        key: 'pinValue',
        value: '0000',
      },
      // </if>
    ];

    // Setup
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);

    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);
    page = document.createElement('print-preview-app');
    document.body.appendChild(page);

    await Promise.all([
      nativeLayer.whenCalled('getInitialSettings'),
      nativeLayer.whenCalled('getPrinterCapabilities'),
    ]);
    // Set all the settings sections.
    testData.forEach((testValue: TestCase, index: number) => {
      if (index === testData.length - 1) {
        nativeLayer.resetResolver('saveAppState');
      }
      // Since advanced options settings doesn't set this setting in
      // production, just use the model instead of creating the dialog.
      const element = testValue.settingName === 'vendorItems' ?
          getInstance() :
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector<SettingsMixinInterface&HTMLElement>(
                  testValue.section)!;
      element.setSetting(testValue.settingName, testValue.value);
    });
    // Wait on only the last call to saveAppState, which should
    // contain all the update settings values.
    const serializedSettingsStr = await nativeLayer.whenCalled('saveAppState');
    const serializedSettings =
        JSON.parse(serializedSettingsStr) as {[key: string]: any};
    // Validate serialized state.
    testData.forEach(testValue => {
      assertEquals(
          JSON.stringify(testValue.value),
          JSON.stringify(serializedSettings[testValue.key]));
    });
  });
});
