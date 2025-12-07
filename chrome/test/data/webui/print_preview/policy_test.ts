// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement, NativeInitialSettings, PolicyObjectEntry, PrintPreviewAppElement, SerializedSettings} from 'chrome://print/print_preview.js';
import {BackgroundGraphicsModeRestriction, NativeLayerImpl, PluginProxyImpl} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getDefaultInitialSettings} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';


interface AllowedDefaultModePolicySetup {
  settingName: string;
  serializedSettingName?: string;
  allowedMode: any;
  defaultMode: any;
}

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
          return microtasksFinished();
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

  function toggleMoreSettings() {
    const moreSettingsElement =
        page.shadowRoot.querySelector('print-preview-sidebar')!.shadowRoot
            .querySelector('print-preview-more-settings')!;
    moreSettingsElement.$.label.click();
  }

  function getCheckbox(settingName: string): CrCheckboxElement {
    return page.shadowRoot.querySelector('print-preview-sidebar')!.shadowRoot
        .querySelector('print-preview-other-options-settings')!.shadowRoot
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
          page.shadowRoot.querySelector('print-preview-sidebar')!.shadowRoot
              .querySelector('print-preview-media-size-settings')!.shadowRoot
              .querySelector('print-preview-settings-select')!.shadowRoot
              .querySelector('select')!;
      assertEquals(
          subtestParams.expectedName,
          JSON.parse(mediaSettingsSelect.value).name);
    }
  });

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
      // <if expr="is_linux">
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
