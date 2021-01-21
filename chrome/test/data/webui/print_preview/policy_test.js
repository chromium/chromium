// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundGraphicsModeRestriction, NativeLayer, NativeLayerImpl, PluginProxyImpl, PrintPreviewPluralStringProxyImpl} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {getCddTemplate, getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {TestPluginProxy} from 'chrome://test/print_preview/test_plugin_proxy.js';
import {TestPluralStringProxy} from 'chrome://test/test_plural_string_proxy.js';

// <if expr="chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

window.policy_tests = {};
policy_tests.suiteName = 'PolicyTest';
/** @enum {string} */
policy_tests.TestNames = {
  HeaderFooterPolicy: 'header/footer policy',
  CssBackgroundPolicy: 'css background policy',
  MediaSizePolicy: 'media size policy',
  SheetsPolicy: 'sheets policy',
};

class PolicyTestPluralStringProxy extends TestPluralStringProxy {
  /** override */
  getPluralString(messageName, itemCount) {
    if (messageName === 'sheetsLimitErrorMessage') {
      this.methodCalled('getPluralString', {messageName, itemCount});
    }
    return Promise.resolve(this.text);
  }
}


suite(policy_tests.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /**
   * @param {!NativeInitialSettings} initialSettings
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function loadInitialSettings(initialSettings) {
    document.body.innerHTML = '';
    const nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    nativeLayer.setPageCount(3);
    NativeLayerImpl.instance_ = nativeLayer;
    // <if expr="chromeos">
    setNativeLayerCrosInstance();
    // </if>
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.instance_ = pluginProxy;

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);

    // Wait for initialization to complete.
    return Promise
        .all([
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities')
        ])
        .then(function() {
          flush();
        });
  }

  /**
   * Sets up the Print Preview app, and loads initial settings with the given
   * policy.
   * @param {string} settingName Name of the setting to set up.
   * @param {string} serializedSettingName Name of the serialized setting.
   * @param {*} allowedMode Allowed value for the given setting.
   * @param {*} defaultMode Default value for the given setting.
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function doAllowedDefaultModePolicySetup(
      settingName, serializedSettingName, allowedMode, defaultMode) {
    const initialSettings = getDefaultInitialSettings();

    if (allowedMode !== undefined || defaultMode !== undefined) {
      const policy = {};
      if (allowedMode !== undefined) {
        policy.allowedMode = allowedMode;
      }
      if (defaultMode !== undefined) {
        policy.defaultMode = defaultMode;
      }
      initialSettings.policies = {[settingName]: policy};
    }
    if (defaultMode !== undefined && serializedSettingName !== undefined) {
      // We want to make sure sticky settings get overridden.
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        [serializedSettingName]: !defaultMode,
      });
    }
    return loadInitialSettings(initialSettings);
  }

  /**
   * Sets up the Print Preview app, and loads initial settings with the
   * given policy.
   * @param {string} settingName Name of the setting to set up.
   * @param {string} serializedSettingName Name of the serialized setting.
   * @param {*} allowedMode Allowed value for the given setting.
   * @param {*} defaultMode Default value for the given setting.
   * @return {!Promise} A Promise that resolves once initial settings are
   *     done loading.
   */
  function doValuePolicySetup(settingName, value) {
    const initialSettings = getDefaultInitialSettings();
    if (value !== undefined) {
      const policy = {value: value};
      initialSettings.policies = {[settingName]: policy};
    }
    return loadInitialSettings(initialSettings);
  }

  function toggleMoreSettings() {
    const moreSettingsElement =
        page.$$('print-preview-sidebar').$$('print-preview-more-settings');
    moreSettingsElement.$.label.click();
  }

  function getCheckbox(settingName) {
    return page.$$('print-preview-sidebar')
        .$$('print-preview-other-options-settings')
        .$$(`#${settingName}`);
  }

  /** Tests different scenarios of applying header/footer policy. */
  test(assert(policy_tests.TestNames.HeaderFooterPolicy), async () => {
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
      }
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'headerFooter', 'isHeaderFooterEnabled', subtestParams.allowedMode,
          subtestParams.defaultMode);
      toggleMoreSettings();
      const checkbox = getCheckbox('headerFooter');
      assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });

  /** Tests different scenarios of applying background graphics policy. */
  test(assert(policy_tests.TestNames.CssBackgroundPolicy), async () => {
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
      }
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'cssBackground', 'isCssBackgroundEnabled', subtestParams.allowedMode,
          subtestParams.defaultMode);
      toggleMoreSettings();
      const checkbox = getCheckbox('cssBackground');
      assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });

  /** Tests different scenarios of applying default paper policy. */
  test(assert(policy_tests.TestNames.MediaSizePolicy), async () => {
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
      }
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'mediaSize', /*serializedSettingName=*/ undefined,
          /*allowedMode=*/ undefined, subtestParams.defaultMode);
      toggleMoreSettings();
      const mediaSettingsSelect = page.$$('print-preview-sidebar')
                                      .$$('print-preview-media-size-settings')
                                      .$$('print-preview-settings-select')
                                      .$$('select');
      assertEquals(
          subtestParams.expectedName,
          JSON.parse(mediaSettingsSelect.value).name);
    }
  });

  test(assert(policy_tests.TestNames.SheetsPolicy), async () => {
    const pluralString = new PolicyTestPluralStringProxy();
    PrintPreviewPluralStringProxyImpl.instance_ = pluralString;
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
      }
    ];
    for (const subtestParams of tests) {
      await doValuePolicySetup('sheets', subtestParams.maxSheets);
      pluralString.resetResolver('getPluralString');
      page.setSetting('pages', subtestParams.pages);
      if (subtestParams.expectedNonEmptyErrorMessage) {
        const {_, itemCount} = await pluralString.whenCalled('getPluralString');
        assertEquals(subtestParams.maxSheets, itemCount);
      }
      const printButton = page.$$('print-preview-sidebar')
                              .$$('print-preview-button-strip')
                              .$$('cr-button.action-button');
      const errorMessage = page.$$('print-preview-sidebar')
                               .$$('print-preview-button-strip')
                               .$$('div.error-message');
      assertEquals(subtestParams.expectedDisabled, printButton.disabled);
      assertEquals(subtestParams.expectedHidden, errorMessage.hidden);
      assertEquals(
          subtestParams.expectedNonEmptyErrorMessage,
          !errorMessage.hidden && !!errorMessage.innerText);
    }
  });
});
