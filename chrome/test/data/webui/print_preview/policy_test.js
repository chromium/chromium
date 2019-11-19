// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayer, PluginProxy} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplate, getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.policy_tests = {};
policy_tests.suiteName = 'PolicyTest';
/** @enum {string} */
policy_tests.TestNames = {
  EnableHeaderFooterByPref: 'enable header and footer by pref',
  DisableHeaderFooterByPref: 'disable header and footer by pref',
  EnableHeaderFooterByPolicy: 'enable header and footer by policy',
  DisableHeaderFooterByPolicy: 'disable header and footer by policy',
};

suite(policy_tests.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /**
   * @param {!NativeInitialSettings} initialSettings
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function loadInitialSettings(initialSettings) {
    const nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate(initialSettings.printerName));
    nativeLayer.setPageCount(3);
    NativeLayer.setInstance(nativeLayer);
    const pluginProxy = new PDFPluginStub();
    PluginProxy.setInstance(pluginProxy);

    PolymerTest.clearBody();
    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    const previewArea = page.$.previewArea;

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
   * prefs/policies.
   * @param headerFooter {boolean} Value for the
   *     'printing.print_header_footer' pref.
   * @param isHeaderFooterManaged {boolean} Whether the
   *     'printing.print_header_footer' is controlled by an enterprise policy.
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function doSetup(headerFooter, isHeaderFooterManaged) {
    const initialSettings = getDefaultInitialSettings();
    initialSettings.headerFooter = headerFooter;
    initialSettings.isHeaderFooterManaged = isHeaderFooterManaged;
    // We want to make sure sticky settings get overridden.
    initialSettings.serializedAppStateStr = JSON.stringify({
      version: 2,
      isHeaderFooterEnabled: !headerFooter,
    });
    return loadInitialSettings(initialSettings);
  }

  function toggleMoreSettings() {
    const moreSettingsElement =
        page.$$('print-preview-sidebar').$$('print-preview-more-settings');
    moreSettingsElement.$.label.click();
  }

  function getCheckbox() {
    return page.$$('print-preview-sidebar')
        .$$('print-preview-other-options-settings')
        .$$('#headerFooter');
  }

  /**
   * Tests that 'printing.print_header_footer' pref checks the header/footer
   * checkbox.
   */
  test(assert(policy_tests.TestNames.EnableHeaderFooterByPref), function() {
    doSetup(true, false).then(function() {
      toggleMoreSettings();
      assertFalse(getCheckbox().disabled);
      assertTrue(getCheckbox().checked);
    });
  });

  /**
   * Tests that 'printing.print_header_footer' pref unchecks the header/footer
   * checkbox.
   */
  test(assert(policy_tests.TestNames.DisableHeaderFooterByPref), function() {
    doSetup(false, false).then(function() {
      toggleMoreSettings();
      assertFalse(getCheckbox().disabled);
      assertFalse(getCheckbox().checked);
    });
  });

  /**
   * Tests that 'force enable header/footer' policy disables the header/footer
   * checkbox and checks it.
   */
  test(assert(policy_tests.TestNames.EnableHeaderFooterByPolicy), function() {
    doSetup(true, true).then(function() {
      toggleMoreSettings();
      assertTrue(getCheckbox().disabled);
      assertTrue(getCheckbox().checked);
    });
  });

  /**
   * Tests that 'force enable header/footer' policy disables the header/footer
   * checkbox and unchecks it.
   */
  test(assert(policy_tests.TestNames.DisableHeaderFooterByPolicy), function() {
    doSetup(false, true).then(function() {
      toggleMoreSettings();
      assertTrue(getCheckbox().disabled);
      assertFalse(getCheckbox().checked);
    });
  });
});
