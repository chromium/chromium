// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, NativeLayer, PluginProxy, whenReady} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isWindows} from 'chrome://resources/js/cr.m.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplate, getDefaultInitialSettings, selectOption} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, waitBeforeNextRender} from 'chrome://test/test_util.m.js';

window.system_dialog_browsertest = {};
system_dialog_browsertest.suiteName = 'SystemDialogBrowserTest';
/** @enum {string} */
system_dialog_browsertest.TestNames = {
  LinkTriggersLocalPrint: 'link triggers local print',
  InvalidSettingsDisableLink: 'invalid settings disable link',
};

suite(system_dialog_browsertest.suiteName, function() {
  /** @type {?PrintPreviewSidebarElement} */
  let sidebar = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {?PrintPreviewLinkContainerElement} */
  let linkContainer = null;

  /** @type {?HTMLElement} */
  let link = null;

  /** @type {string} */
  let printTicketKey = '';

  /** @override */
  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);
    PolymerTest.clearBody();

    const initialSettings = getDefaultInitialSettings();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate(initialSettings.printerName));
    const pluginProxy = new PDFPluginStub();
    PluginProxy.setInstance(pluginProxy);

    const page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    const previewArea = page.$.previewArea;
    sidebar = page.$$('print-preview-sidebar');
    return Promise
        .all([
          waitBeforeNextRender(page),
          whenReady(),
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities'),
        ])
        .then(function() {
          linkContainer = sidebar.$$('print-preview-link-container');
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function() {
          assertEquals('FooDevice', page.destination_.id);
          link = isWindows ? linkContainer.$.systemDialogLink :
                             linkContainer.$.openPdfInPreviewLink;
          printTicketKey = isWindows ? 'showSystemDialog' : 'OpenPDFInPreview';
        });
  });

  test(
      assert(system_dialog_browsertest.TestNames.LinkTriggersLocalPrint),
      function() {
        assertFalse(linkContainer.disabled);
        assertFalse(link.hidden);
        link.click();
        // Should result in a print call and dialog should close.
        return nativeLayer.whenCalled('print').then(function(printTicket) {
          expectTrue(JSON.parse(printTicket)[printTicketKey]);
          return nativeLayer.whenCalled('dialogClose');
        });
      });

  test(
      assert(system_dialog_browsertest.TestNames.InvalidSettingsDisableLink),
      function() {
        assertFalse(linkContainer.disabled);
        assertFalse(link.hidden);

        const moreSettingsElement = sidebar.$$('print-preview-more-settings');
        moreSettingsElement.$.label.click();
        const scalingSettings = sidebar.$$('print-preview-scaling-settings');
        assertFalse(scalingSettings.hidden);
        nativeLayer.resetResolver('getPreview');
        let previewCalls = 0;

        // Set scaling settings to custom.
        return selectOption(
                   scalingSettings,
                   scalingSettings.ScalingValue.CUSTOM.toString())
            .then(() => {
              previewCalls = nativeLayer.getCallCount('getPreview');

              // Set an invalid input.
              const scalingSettingsInput =
                  scalingSettings.$$('print-preview-number-settings-section')
                      .$.userValue.inputElement;
              scalingSettingsInput.value = '0';
              scalingSettingsInput.dispatchEvent(
                  new CustomEvent('input', {composed: true, bubbles: true}));

              return eventToPromise('input-change', scalingSettings);
            })
            .then(() => {
              // Expect disabled print button
              const parentElement = sidebar.$$('print-preview-button-strip');
              const printButton = parentElement.$$('.action-button');
              assertTrue(printButton.disabled);
              assertTrue(linkContainer.disabled);
              assertFalse(link.hidden);
              assertTrue(link.querySelector('cr-icon-button').disabled);

              // No new preview
              assertEquals(
                  previewCalls, nativeLayer.getCallCount('getPreview'));
            });
      });
});
