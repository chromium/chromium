// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement, NativeInitialSettings, PolicyObjectEntry, PrintPreviewAppElement, SerializedSettings} from 'chrome://print/print_preview.js';
import {BackgroundGraphicsModeRestriction, NativeLayerImpl, PluginProxyImpl} from 'chrome://print/print_preview.js';
// <if expr="is_chromeos">
import type {CrButtonElement} from 'chrome://print/print_preview.js';
import {ColorModeRestriction, DuplexMode, DuplexModeRestriction, PinModeRestriction, PrintPreviewPluralStringProxyImpl} from 'chrome://print/print_preview.js';
// </if>

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
// <if expr="is_chromeos">
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';

// </if>

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {getDefaultInitialSettings} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';


interface AllowedDefaultModePolicySetup {
  settingName: string;
  serializedSettingName?: string;
  allowedMode: any;
  defaultMode: any;
}

// <if expr="is_chromeos">
class PolicyTestPluralStringProxy extends TestPluralStringProxy {
  override text: string = '';

  override getPluralString(messageName: string, itemCount: number) {
    if (messageName === 'sheetsLimitErrorMessage') {
      this.methodCalled('getPluralString', {messageName, itemCount});
    }
    return Promise.resolve(this.text);
  }
}
// </if>

suite('PolicyTest', function() {
  let page: PrintPreviewAppElement;

  /**
   * @return A Promise that resolves once initial settings are done
   *     loading.
   */
  function loadInitialSettings(initialSettings: NativeInitialSettings):
      Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    nativeLayer.setPageCount(3);
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);

    // Wait for initialization to complete.
    return Promise
        .all([
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities'),
        ])
        .then(function() {
          flush();
        });
  }

  /**
   * Sets up the Print Preview app, and loads initial settings with the given
   * policy.
   * @param policies Policies to set up.
   * @param isPdf If settings are for previewing a PDF.
   * @return A Promise that resolves once initial settings are done loading.
   */
  function doAllowedDefaultModePoliciesSetup(
      policies: AllowedDefaultModePolicySetup[],
      isPdf: boolean = false): Promise<void> {
    const initialSettings = getDefaultInitialSettings(isPdf);
    const appState: SerializedSettings = {version: 2};
    let setSerializedAppState = false;

    initialSettings.policies = {};

    policies.forEach(setup => {
      if (setup.allowedMode !== undefined || setup.defaultMode !== undefined) {
        const policy: PolicyObjectEntry = {};
        if (setup.allowedMode !== undefined) {
          policy.allowedMode = setup.allowedMode;
        }
        if (setup.defaultMode !== undefined) {
          policy.defaultMode = setup.defaultMode;
        }
        (initialSettings.policies as {[key: string]: any})[setup.settingName] =
            policy;
      }
      if (setup.defaultMode !== undefined &&
          setup.serializedSettingName !== undefined) {
        // We want to make sure sticky settings get overridden.
        (appState as {[key: string]: any})[setup.serializedSettingName] =
            !setup.defaultMode;
        setSerializedAppState = true;
      }
    });

    if (setSerializedAppState) {
      initialSettings.serializedAppStateStr = JSON.stringify(appState);
    }
    return loadInitialSettings(initialSettings);
  }

  // <if expr="is_chromeos">
  /**
   * Sets up the Print Preview app, and loads initial settings with the
   * given policy.
   * @param settingName Name of the setting to set up.
   * @param value The value to set for the policy.
   * @return A Promise that resolves once initial settings are done loading.
   */
  function doValuePolicySetup(settingName: string, value: any): Promise<void> {
    const initialSettings = getDefaultInitialSettings();
    if (value !== undefined) {
      const policy: PolicyObjectEntry = {value: value};
      initialSettings.policies = {[settingName]: policy};
    }
    return loadInitialSettings(initialSettings);
  }
  // </if>

  function toggleMoreSettings() {
    const moreSettingsElement =
        page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
            .querySelector('print-preview-more-settings')!;
    moreSettingsElement.$.label.click();
  }

  function getCheckbox(settingName: string): CrCheckboxElement {
    return page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
        .querySelector('print-preview-other-options-settings')!.shadowRoot!
        .querySelector<CrCheckboxElement>(`#${settingName}`)!;
  }

  // Tests different scenarios of applying header/footer policy.
  test('HeaderFooterPolicy', async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedDisabled: false,
        expectedChecked: true,
      },
      {
        // Restrict header/footer to be enabled.
        allowedMode: true,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedChecked: true,
      },
      {
        // Restrict header/footer to be disabled.
        allowedMode: false,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedChecked: false,
      },
      {
        // Check header/footer checkbox.
        allowedMode: undefined,
        defaultMode: true,
        expectedDisabled: false,
        expectedChecked: true,
      },
      {
        // Uncheck header/footer checkbox.
        allowedMode: undefined,
        defaultMode: false,
        expectedDisabled: false,
        expectedChecked: false,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup([{
        settingName: 'headerFooter',
        serializedSettingName: 'isHeaderFooterEnabled',
        allowedMode: subtestParams.allowedMode,
        defaultMode: subtestParams.defaultMode,
      }]);
      toggleMoreSettings();
      const checkbox = getCheckbox('headerFooter');
      assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });

  // Tests different scenarios of applying background graphics policy.
  test('CssBackgroundPolicy', async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedDisabled: false,
        expectedChecked: false,
      },
      {
        // Restrict background graphics to be enabled.
        // Check that checkbox value default mode is not applied if it
        // contradicts allowed mode.
        allowedMode: BackgroundGraphicsModeRestriction.ENABLED,
        defaultMode: BackgroundGraphicsModeRestriction.DISABLED,
        expectedDisabled: true,
        expectedChecked: true,
      },
      {
        // Restrict background graphics to be disabled.
        allowedMode: BackgroundGraphicsModeRestriction.DISABLED,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedChecked: false,
      },
      {
        // Check background graphics checkbox.
        allowedMode: undefined,
        defaultMode: BackgroundGraphicsModeRestriction.ENABLED,
        expectedDisabled: false,
        expectedChecked: true,
      },
      {
        // Uncheck background graphics checkbox.
        allowedMode: BackgroundGraphicsModeRestriction.UNSET,
        defaultMode: BackgroundGraphicsModeRestriction.DISABLED,
        expectedDisabled: false,
        expectedChecked: false,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup([{
        settingName: 'cssBackground',
        serializedSettingName: 'isCssBackgroundEnabled',
        allowedMode: subtestParams.allowedMode,
        defaultMode: subtestParams.defaultMode,
      }]);
      toggleMoreSettings();
      const checkbox = getCheckbox('cssBackground');
      assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });

  // Tests different scenarios of applying default paper policy.
  test('MediaSizePolicy', async () => {
    const tests = [
      {
        // No policies.
        defaultMode: undefined,
        expectedName: 'NA_LETTER',
      },
      {
        // Not available option shouldn't change actual paper size setting.
        defaultMode: {width: 200000, height: 200000},
        expectedName: 'NA_LETTER',
      },
      {
        // Change default paper size setting.
        defaultMode: {width: 215900, height: 215900},
        expectedName: 'CUSTOM',
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup([{
        settingName: 'mediaSize',
        serializedSettingName: undefined,
        allowedMode: undefined,
        defaultMode: subtestParams.defaultMode,
      }]);
      toggleMoreSettings();
      const mediaSettingsSelect =
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector('print-preview-media-size-settings')!.shadowRoot!
              .querySelector('print-preview-settings-select')!.shadowRoot!
              .querySelector('select')!;
      assertEquals(
          subtestParams.expectedName,
          JSON.parse(mediaSettingsSelect.value).name);
    }
  });

  // <if expr="is_chromeos">
  test('SheetsPolicy', async () => {
    const pluralString = new PolicyTestPluralStringProxy();
    PrintPreviewPluralStringProxyImpl.setInstance(pluralString);
    pluralString.text = 'Exceeds limit of 1 sheet of paper';

    const tests = [
      {
        // No policy.
        maxSheets: 0,
        pages: [1, 2, 3],
        expectedDisabled: false,
        expectedHidden: true,
        expectedNonEmptyErrorMessage: false,
      },
      {
        // Policy is set, actual pages are not calculated yet.
        maxSheets: 3,
        pages: [],
        expectedDisabled: true,
        expectedHidden: true,
        expectedNonEmptyErrorMessage: false,
      },
      {
        // Policy is set, but the limit is not hit.
        maxSheets: 3,
        pages: [1, 2],
        expectedDisabled: false,
        expectedHidden: true,
        expectedNonEmptyErrorMessage: false,
      },
      {
        // Policy is set, the limit is hit, singular form is used.
        maxSheets: 1,
        pages: [1, 2],
        expectedDisabled: true,
        expectedHidden: false,
        expectedNonEmptyErrorMessage: true,
      },
      {
        // Policy is set, the limit is hit, plural form is used.
        maxSheets: 2,
        pages: [1, 2, 3, 4],
        expectedDisabled: true,
        expectedHidden: false,
        expectedNonEmptyErrorMessage: true,
      },
    ];
    for (const subtestParams of tests) {
      await doValuePolicySetup('sheets', subtestParams.maxSheets);
      pluralString.resetResolver('getPluralString');
      page.setSetting('pages', subtestParams.pages);
      if (subtestParams.expectedNonEmptyErrorMessage) {
        const pluralStringArgs =
            await pluralString.whenCalled('getPluralString');
        assertEquals(subtestParams.maxSheets, pluralStringArgs.itemCount);
      }
      const printButton =
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector('print-preview-button-strip')!.shadowRoot!
              .querySelector<CrButtonElement>('cr-button.action-button')!;
      const errorMessage =
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector('print-preview-button-strip')!.shadowRoot!
              .querySelector<HTMLElement>('div.error-message')!;
      assertEquals(subtestParams.expectedDisabled, printButton.disabled);
      assertEquals(subtestParams.expectedHidden, errorMessage.hidden);
      assertEquals(
          subtestParams.expectedNonEmptyErrorMessage,
          !errorMessage.hidden && !!errorMessage.innerText);
    }
  });

  // Tests different scenarios of color printing policy.
  test('ColorPolicy', async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedDisabled: false,
        expectedValue: 'color',
      },
      {
        // Print in color by default.
        allowedMode: undefined,
        defaultMode: ColorModeRestriction.COLOR,
        expectedDisabled: false,
        expectedValue: 'color',
      },
      {
        // Print in black and white by default.
        allowedMode: undefined,
        defaultMode: ColorModeRestriction.MONOCHROME,
        expectedDisabled: false,
        expectedValue: 'bw',
      },
      {
        // Allowed and default policies unset.
        allowedMode: ColorModeRestriction.UNSET,
        defaultMode: ColorModeRestriction.UNSET,
        expectedDisabled: false,
        expectedValue: 'bw',
      },
      {
        // Allowed unset, default set to color printing.
        allowedMode: ColorModeRestriction.UNSET,
        defaultMode: ColorModeRestriction.COLOR,
        expectedDisabled: false,
        expectedValue: 'color',
      },
      {
        // Enforce color printing.
        allowedMode: ColorModeRestriction.COLOR,
        defaultMode: ColorModeRestriction.UNSET,
        expectedDisabled: true,
        expectedValue: 'color',
      },
      {
        // Enforce black and white printing.
        allowedMode: ColorModeRestriction.MONOCHROME,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedValue: 'bw',
      },
      {
        // Enforce color printing, default is ignored.
        allowedMode: ColorModeRestriction.COLOR,
        defaultMode: ColorModeRestriction.MONOCHROME,
        expectedDisabled: true,
        expectedValue: 'color',
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup([{
        settingName: 'color',
        serializedSettingName: 'isColorEnabled',
        allowedMode: subtestParams.allowedMode,
        defaultMode: subtestParams.defaultMode,
      }]);
      const colorSettingsSelect =
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector('print-preview-color-settings')!.shadowRoot!
              .querySelector('select')!;
      assertEquals(
          subtestParams.expectedDisabled, colorSettingsSelect.disabled);
      assertEquals(subtestParams.expectedValue, colorSettingsSelect.value);
    }
  });

  // Tests different scenarios of duplex printing policy.
  test('DuplexPolicy', async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to SIMPLEX.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.SIMPLEX,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to UNSET.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.UNSET,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Allowed mode set to UNSET.
        allowedMode: DuplexModeRestriction.UNSET,
        defaultMode: undefined,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Allowed mode set to UNSET, default set to LONG_EDGE.
        allowedMode: DuplexModeRestriction.UNSET,
        defaultMode: DuplexModeRestriction.LONG_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Allowed mode set to UNSET, default set to SHORT_EDGE.
        allowedMode: DuplexModeRestriction.UNSET,
        defaultMode: DuplexModeRestriction.SHORT_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.SHORT_EDGE,
      },
      {
        // Restricted to SIMPLEX.
        allowedMode: DuplexModeRestriction.SIMPLEX,
        defaultMode: DuplexModeRestriction.UNSET,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to DUPLEX.
        allowedMode: DuplexModeRestriction.DUPLEX,
        defaultMode: DuplexModeRestriction.UNSET,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to DUPLEX, default set to SHORT_EDGE.
        allowedMode: DuplexModeRestriction.DUPLEX,
        defaultMode: DuplexModeRestriction.SHORT_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.SHORT_EDGE,
      },
      {
        // Restricted to DUPLEX, default set to SHORT_EDGE.
        allowedMode: DuplexModeRestriction.DUPLEX,
        defaultMode: DuplexModeRestriction.LONG_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to DUPLEX, default is ignored.
        allowedMode: DuplexModeRestriction.DUPLEX,
        defaultMode: DuplexModeRestriction.SIMPLEX,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to SIMPLEX, default is ignored.
        allowedMode: DuplexModeRestriction.SIMPLEX,
        defaultMode: DuplexModeRestriction.LONG_EDGE,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup([{
        settingName: 'duplex',
        serializedSettingName: 'isDuplexEnabled',
        allowedMode: subtestParams.allowedMode,
        defaultMode: subtestParams.defaultMode,
      }]);
      toggleMoreSettings();
      const duplexSettingsSection =
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector('print-preview-duplex-settings')!;
      const checkbox =
          duplexSettingsSection.shadowRoot!.querySelector('cr-checkbox')!;
      const collapse =
          duplexSettingsSection.shadowRoot!.querySelector('cr-collapse')!;
      const select = duplexSettingsSection.shadowRoot!.querySelector('select')!;
      const expectedValue = subtestParams.expectedValue.toString();
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
      assertEquals(subtestParams.expectedOpened, collapse.opened);
      assertEquals(subtestParams.expectedDisabled, select.disabled);
      assertEquals(expectedValue, select.value);
    }
  });

  // Tests different scenarios of pin printing policy.
  test('PinPolicy', async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // No restriction, default set to UNSET.
        allowedMode: undefined,
        defaultMode: PinModeRestriction.UNSET,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // No restriction, default set to PIN.
        allowedMode: undefined,
        defaultMode: PinModeRestriction.PIN,
        expectedCheckboxDisabled: false,
        expectedChecked: true,
        expectedOpened: true,
        expectedInputDisabled: false,
      },
      {
        // No restriction, default set to NO_PIN.
        allowedMode: undefined,
        defaultMode: PinModeRestriction.NO_PIN,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // Restriction se to UNSET.
        allowedMode: PinModeRestriction.UNSET,
        defaultMode: undefined,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // Restriction set to PIN.
        allowedMode: PinModeRestriction.PIN,
        defaultMode: undefined,
        expectedCheckboxDisabled: true,
        expectedChecked: true,
        expectedOpened: true,
        expectedInputDisabled: false,
      },
      {
        // Restriction set to NO_PIN.
        allowedMode: PinModeRestriction.NO_PIN,
        defaultMode: undefined,
        expectedCheckboxDisabled: true,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // Restriction set to PIN, default is ignored.
        allowedMode: PinModeRestriction.NO_PIN,
        defaultMode: PinModeRestriction.PIN,
        expectedCheckboxDisabled: true,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
    ];
    for (const subtestParams of tests) {
      const initialSettings = getDefaultInitialSettings();

      if (subtestParams.allowedMode !== undefined ||
          subtestParams.defaultMode !== undefined) {
        const policy: PolicyObjectEntry = {};
        if (subtestParams.allowedMode !== undefined) {
          policy.allowedMode = subtestParams.allowedMode;
        }
        if (subtestParams.defaultMode !== undefined) {
          policy.defaultMode = subtestParams.defaultMode;
        }
        initialSettings.policies = {'pin': policy};
      }

      const appState: SerializedSettings = {version: 2, 'pinValue': '0000'};
      if (subtestParams.defaultMode !== undefined) {
        appState.isPinEnabled = !subtestParams.defaultMode;
      }
      initialSettings.serializedAppStateStr = JSON.stringify(appState);

      await loadInitialSettings(initialSettings);

      const pinSettingsSection =
          page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
              .querySelector('print-preview-pin-settings')!;
      const checkbox =
          pinSettingsSection.shadowRoot!.querySelector('cr-checkbox')!;
      const collapse =
          pinSettingsSection.shadowRoot!.querySelector('cr-collapse')!;
      const input = pinSettingsSection.shadowRoot!.querySelector('cr-input')!;
      assertEquals(subtestParams.expectedCheckboxDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
      assertEquals(subtestParams.expectedOpened, collapse.opened);
      assertEquals(subtestParams.expectedInputDisabled, input.disabled);
    }
  });
  // </if>

  // <if expr="is_win or is_macosx">
  // Tests different scenarios of PDF print as image option policy.
  // Should be available only for PDF when the policy explicitly allows print
  // as image, and hidden the rest of the cases.
  test('PrintPdfAsImageAvailability', async () => {
    const tests = [
      {
        // No policies with modifiable content.
        allowedMode: undefined,
        isPdf: false,
        expectedHidden: true,
      },
      {
        // No policies with PDF content.
        allowedMode: undefined,
        isPdf: true,
        expectedHidden: true,
      },
      {
        // Explicitly restrict "Print as image" option for modifiable content.
        allowedMode: false,
        isPdf: false,
        expectedHidden: true,
      },
      {
        // Explicitly restrict "Print as image" option for PDF content.
        allowedMode: false,
        isPdf: true,
        expectedHidden: true,
      },
      {
        // Explicitly enable "Print as image" option for modifiable content.
        allowedMode: true,
        isPdf: false,
        expectedHidden: true,
      },
      {
        // Explicitly enable "Print as image" option for PDF content.
        allowedMode: true,
        isPdf: true,
        expectedHidden: false,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup(
          [{
            settingName: 'printPdfAsImageAvailability',
            serializedSettingName: 'isRasterizeEnabled',
            allowedMode: subtestParams.allowedMode,
            defaultMode: undefined,
          }],
          /*isPdf=*/ subtestParams.isPdf);
      toggleMoreSettings();
      const checkbox = getCheckbox('rasterize');
      assertEquals(
          subtestParams.expectedHidden,
          (checkbox.parentNode!.parentNode! as HTMLElement).hidden);
    }
  });
  // </if>

  // Tests different scenarios of PDF "Print as image" option default policy.
  // Default only has an effect when the "Print as image" option is available.
  // The policy controls if it defaults to set.Test behavior varies by platform
  // since the option's availability is policy controlled for Windows and macOS
  // but is always available for Linux and ChromeOS.
  test('PrintPdfAsImageDefault', async () => {
    const tests = [
      // <if expr="is_linux or chromeos_ash or chromeos_lacros">
      {
        // `availableAllowedMode` is irrelevant, option is always present.
        // No policy for default of "Print as image" option.
        availableAllowedMode: undefined,
        selectedDefaultMode: undefined,
        expectedChecked: false,
      },
      {
        // `availableAllowedMode` is irrelevant, option is always present.
        // Explicitly default "Print as image" to unset for PDF content.
        availableAllowedMode: undefined,
        selectedDefaultMode: false,
        expectedChecked: false,
      },
      {
        // `availableAllowedMode` is irrelevant, option is always present.
        // Explicitly default "Print as image" to set for PDF content.
        availableAllowedMode: undefined,
        selectedDefaultMode: true,
        expectedChecked: true,
      },
      // </if>
      {
        // Explicitly enable "Print as image" option for PDF content.
        // No policy for default of "Print as image" option.
        availableAllowedMode: true,
        selectedDefaultMode: undefined,
        expectedChecked: false,
      },
      {
        // Explicitly enable "Print as image" option for PDF content.
        // Explicitly default "Print as image" to unset.
        availableAllowedMode: true,
        selectedDefaultMode: false,
        expectedChecked: false,
      },
      {
        // Explicitly enable "Print as image" option for PDF content.
        // Explicitly default "Print as image" to set.
        availableAllowedMode: true,
        selectedDefaultMode: true,
        expectedChecked: true,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePoliciesSetup(
          [
            {
              settingName: 'printPdfAsImageAvailability',
              serializedSettingName: 'isRasterizeEnabled',
              allowedMode: subtestParams.availableAllowedMode,
              defaultMode: undefined,
            },
            {
              settingName: 'printPdfAsImage',
              serializedSettingName: undefined,
              allowedMode: undefined,
              defaultMode: subtestParams.selectedDefaultMode,
            },
          ],
          /*isPdf=*/ true);
      toggleMoreSettings();
      const checkbox = getCheckbox('rasterize');
      assertFalse((checkbox.parentNode!.parentNode! as HTMLElement).hidden);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });
});
