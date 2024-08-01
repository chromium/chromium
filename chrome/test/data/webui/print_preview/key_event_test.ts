// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewAppElement} from 'chrome://print/print_preview.js';
import {NativeLayerImpl, PluginProxyImpl} from 'chrome://print/print_preview.js';
import {isChromeOS, isLacros, isMac, isWindows} from 'chrome://resources/js/platform.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>
import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplateWithAdvancedSettings, getDefaultInitialSettings} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('KeyEventTest', function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

  setup(function() {
    const initialSettings = getDefaultInitialSettings();
    nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    // Use advanced settings so that we can test with the cr-button.
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplateWithAdvancedSettings(1, initialSettings.printerName));
    nativeLayer.setPageCount(3);
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
  });

  // Tests that the enter key triggers a call to print.
  test('EnterTriggersPrint', function() {
    const whenPrintCalled = nativeLayer.whenCalled('doPrint');
    keyEventOn(page, 'keydown', 0, [], 'Enter');
    return whenPrintCalled;
  });

  // Tests that the numpad enter key triggers a call to print.
  test('NumpadEnterTriggersPrint', function() {
    const whenPrintCalled = nativeLayer.whenCalled('doPrint');
    keyEventOn(page, 'keydown', 0, [], 'Enter');
    return whenPrintCalled;
  });

  // Tests that the enter key triggers a call to print if an input is the
  // source of the event.
  test('EnterOnInputTriggersPrint', function() {
    const whenPrintCalled = nativeLayer.whenCalled('doPrint');
    keyEventOn(
        page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
            .querySelector('print-preview-copies-settings')!.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.shadowRoot!
            .querySelector('cr-input')!.inputElement,
        'keydown', 0, [], 'Enter');
    return whenPrintCalled;
  });

  // Tests that the enter key does not trigger a call to print if the event
  // comes from a dropdown.
  test(
      'EnterOnDropdownDoesNotPrint', function() {
        const whenKeyEventFired = eventToPromise('keydown', page);
        keyEventOn(
            page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
                .querySelector('print-preview-layout-settings')!.shadowRoot!
                .querySelector<HTMLSelectElement>('.md-select')!,
            'keydown', 0, [], 'Enter');
        return whenKeyEventFired.then(
            () => assertEquals(0, nativeLayer.getCallCount('doPrint')));
      });

  // Tests that the enter key does not trigger a call to print if the event
  // comes from a button.
  test('EnterOnButtonDoesNotPrint', async () => {
    const moreSettingsElement =
        page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
            .querySelector('print-preview-more-settings')!;
    moreSettingsElement.$.label.click();
    const button =
        page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
            .querySelector('print-preview-advanced-options-settings')!
            .shadowRoot!.querySelector('cr-button')!;
    const whenKeyEventFired = eventToPromise('keydown', button);
    keyEventOn(button, 'keydown', 0, [], 'Enter');
    await whenKeyEventFired;
    await flushTasks();
    assertEquals(0, nativeLayer.getCallCount('doPrint'));
  });

  // Tests that the enter key does not trigger a call to print if the event
  // comes from a checkbox.
  test(
      'EnterOnCheckboxDoesNotPrint', function() {
        const moreSettingsElement =
            page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
                .querySelector('print-preview-more-settings')!;
        moreSettingsElement.$.label.click();
        const whenKeyEventFired = eventToPromise('keydown', page);
        keyEventOn(
            page.shadowRoot!.querySelector('print-preview-sidebar')!.shadowRoot!
                .querySelector('print-preview-other-options-settings')!
                .shadowRoot!.querySelector('cr-checkbox')!,
            'keydown', 0, [], 'Enter');
        return whenKeyEventFired.then(
            () => assertEquals(0, nativeLayer.getCallCount('doPrint')));
      });

  // Tests that escape closes the dialog only on Mac.
  test(
      'EscapeClosesDialogOnMacOnly', function() {
        const promise = isMac ?
            nativeLayer.whenCalled('dialogClose') :
            eventToPromise('keydown', page).then(() => {
              assertEquals(0, nativeLayer.getCallCount('dialogClose'));
            });
        keyEventOn(page, 'keydown', 0, [], 'Escape');
        return promise;
      });

  // Tests that Cmd + Period closes the dialog only on Mac
  test(
      'CmdPeriodClosesDialogOnMacOnly', function() {
        const promise = isMac ?
            nativeLayer.whenCalled('dialogClose') :
            eventToPromise('keydown', page).then(() => {
              assertEquals(0, nativeLayer.getCallCount('dialogClose'));
            });
        keyEventOn(page, 'keydown', 0, ['meta'], '.');
        return promise;
      });

  // Tests that Ctrl+Shift+P opens the system dialog.
  test(
      'CtrlShiftPOpensSystemDialog', function() {
        let promise: Promise<void>;
        if (isChromeOS || isLacros) {
          // Chrome OS doesn't have a system dialog. Just make sure the key
          // event does not trigger a crash.
          promise = Promise.resolve();
        } else if (isWindows) {
          promise = nativeLayer.whenCalled('doPrint').then((printTicket) => {
            assertTrue(JSON.parse(printTicket).showSystemDialog);
          });
        } else {
          promise = nativeLayer.whenCalled('showSystemDialog');
        }
        const modifiers: ModifiersParam =
            isMac ? ['meta', 'alt'] : ['ctrl', 'shift'];
        const key = isMac ? '\u03c0' : 'P';
        keyEventOn(page, 'keydown', 0, modifiers, key);
        return promise;
      });
});
