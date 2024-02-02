// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement, PrintPreviewLinkContainerElement, PrintPreviewSidebarElement} from 'chrome://print/print_preview.js';
import {NativeLayerImpl, PluginProxyImpl, ScalingType, whenReady} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getDefaultInitialSettings, selectOption} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('SystemDialogTest', function() {
  let sidebar: PrintPreviewSidebarElement;

  let nativeLayer: NativeLayerStub;

  let linkContainer: PrintPreviewLinkContainerElement;

  let link: HTMLElement;

  // <if expr="is_win">
  let printTicketKey: string = 'showSystemDialog';
  // </if>
  // <if expr="is_macosx">
  let printTicketKey: string = 'openPDFInPreview';
  // </if>

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const initialSettings = getDefaultInitialSettings();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

    const page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    return Promise
        .all([
          waitBeforeNextRender(page),
          whenReady(),
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities'),
        ])
        .then(function() {
          linkContainer = sidebar.shadowRoot!.querySelector(
              'print-preview-link-container')!;
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function() {
          assertEquals(
              'FooDevice',
              sidebar.shadowRoot!
                  .querySelector(
                      'print-preview-destination-settings')!.destination!.id);
          // <if expr="is_win">
          link = linkContainer.$.systemDialogLink;
          // </if>
          // <if expr="is_macosx">
          link = linkContainer.$.openPdfInPreviewLink;
          // </if>
        });
  });

  test('LinkTriggersLocalPrint', function() {
    assertFalse(linkContainer.disabled);
    assertFalse(link.hidden);
    link.click();
    // Should result in a print call and dialog should close.
    return nativeLayer.whenCalled('doPrint').then((printTicket: string) => {
      assertTrue(JSON.parse(printTicket)[printTicketKey]);
      return nativeLayer.whenCalled('dialogClose');
    });
  });

  test('InvalidSettingsDisableLink', function() {
    assertFalse(linkContainer.disabled);
    assertFalse(link.hidden);

    const moreSettingsElement =
        sidebar.shadowRoot!.querySelector('print-preview-more-settings')!;
    moreSettingsElement.$.label.click();
    const scalingSettings =
        sidebar.shadowRoot!.querySelector('print-preview-scaling-settings')!;
    assertFalse(scalingSettings.hidden);
    nativeLayer.resetResolver('getPreview');
    let previewCalls = 0;

    // Set scaling settings to custom.
    return selectOption(scalingSettings, ScalingType.CUSTOM.toString())
        .then(() => {
          previewCalls = nativeLayer.getCallCount('getPreview');

          // Set an invalid input.
          const scalingSettingsInput =
              scalingSettings.shadowRoot!
                  .querySelector('print-preview-number-settings-section')!.$
                  .userValue.inputElement;
          scalingSettingsInput.value = '0';
          scalingSettingsInput.dispatchEvent(
              new CustomEvent('input', {composed: true, bubbles: true}));

          return eventToPromise('input-change', scalingSettings);
        })
        .then(() => {
          // Expect disabled print button
          const parentElement =
              sidebar.shadowRoot!.querySelector('print-preview-button-strip')!;
          const printButton =
              parentElement.shadowRoot!.querySelector<CrButtonElement>(
                  '.action-button')!;
          assertTrue(printButton.disabled);
          assertTrue(linkContainer.disabled);
          assertFalse(link.hidden);
          assertTrue(link.querySelector('cr-icon-button')!.disabled);

          // No new preview
          assertEquals(previewCalls, nativeLayer.getCallCount('getPreview'));
        });
  });
});
