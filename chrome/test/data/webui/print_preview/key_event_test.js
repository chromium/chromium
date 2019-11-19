// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayer, PluginProxy} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplateWithAdvancedSettings, getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

window.key_event_test = {};
key_event_test.suiteName = 'KeyEventTest';
/** @enum {string} */
key_event_test.TestNames = {
  EnterTriggersPrint: 'enter triggers print',
  NumpadEnterTriggersPrint: 'numpad enter triggers print',
  EnterOnInputTriggersPrint: 'enter on input triggers print',
  EnterOnDropdownDoesNotPrint: 'enter on dropdown does not print',
  EnterOnButtonDoesNotPrint: 'enter on button does not print',
  EnterOnCheckboxDoesNotPrint: 'enter on checkbox does not print',
  EscapeClosesDialogOnMacOnly: 'escape closes dialog on mac only',
  CmdPeriodClosesDialogOnMacOnly: 'cmd period closes dialog on mac only',
  CtrlShiftPOpensSystemDialog: 'ctrl shift p opens system dialog',
};

suite(key_event_test.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /** @type {?PrintPreviewNativeLayer} */
  let nativeLayer = null;

  /** @override */
  setup(function() {
    const initialSettings = getDefaultInitialSettings();
    nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    // Use advanced settings so that we can test with the cr-button.
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplateWithAdvancedSettings(1, initialSettings.printerName));
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
  });

  // Tests that the enter key triggers a call to print.
  test(assert(key_event_test.TestNames.EnterTriggersPrint), function() {
    const whenPrintCalled = nativeLayer.whenCalled('print');
    keyEventOn(page, 'keydown', 'Enter', [], 'Enter');
    return whenPrintCalled;
  });

  // Tests that the numpad enter key triggers a call to print.
  test(assert(key_event_test.TestNames.NumpadEnterTriggersPrint), function() {
    const whenPrintCalled = nativeLayer.whenCalled('print');
    keyEventOn(page, 'keydown', 'NumpadEnter', [], 'Enter');
    return whenPrintCalled;
  });

  // Tests that the enter key triggers a call to print if an input is the
  // source of the event.
  test(assert(key_event_test.TestNames.EnterOnInputTriggersPrint), function() {
    const whenPrintCalled = nativeLayer.whenCalled('print');
    keyEventOn(
        page.$$('print-preview-sidebar')
            .$$('print-preview-copies-settings')
            .$$('print-preview-number-settings-section')
            .$$('cr-input')
            .inputElement,
        'keydown', 'Enter', [], 'Enter');
    return whenPrintCalled;
  });

  // Tests that the enter key does not trigger a call to print if the event
  // comes from a dropdown.
  test(
      assert(key_event_test.TestNames.EnterOnDropdownDoesNotPrint), function() {
        const whenKeyEventFired = eventToPromise('keydown', page);
        keyEventOn(
            page.$$('print-preview-sidebar')
                .$$('print-preview-layout-settings')
                .$$('.md-select'),
            'keydown', 'Enter', [], 'Enter');
        return whenKeyEventFired.then(
            () => assertEquals(0, nativeLayer.getCallCount('print')));
      });

  // Tests that the enter key does not trigger a call to print if the event
  // comes from a button.
  test(assert(key_event_test.TestNames.EnterOnButtonDoesNotPrint), async () => {
    const moreSettingsElement =
        page.$$('print-preview-sidebar').$$('print-preview-more-settings');
    moreSettingsElement.$.label.click();
    const button = page.$$('print-preview-sidebar')
                       .$$('print-preview-advanced-options-settings')
                       .$$('cr-button');
    const whenKeyEventFired = eventToPromise('keydown', button);
    keyEventOn(button, 'keydown', 'Enter', [], 'Enter');
    await whenKeyEventFired;
    await flushTasks();
    assertEquals(0, nativeLayer.getCallCount('print'));
  });

  // Tests that the enter key does not trigger a call to print if the event
  // comes from a checkbox.
  test(
      assert(key_event_test.TestNames.EnterOnCheckboxDoesNotPrint), function() {
        const moreSettingsElement =
            page.$$('print-preview-sidebar').$$('print-preview-more-settings');
        moreSettingsElement.$.label.click();
        const whenKeyEventFired = eventToPromise('keydown', page);
        keyEventOn(
            page.$$('print-preview-sidebar')
                .$$('print-preview-other-options-settings')
                .$$('cr-checkbox'),
            'keydown', 'Enter', [], 'Enter');
        return whenKeyEventFired.then(
            () => assertEquals(0, nativeLayer.getCallCount('print')));
      });

  // Tests that escape closes the dialog only on Mac.
  test(
      assert(key_event_test.TestNames.EscapeClosesDialogOnMacOnly), function() {
        const promise = isMac ?
            nativeLayer.whenCalled('dialogClose') :
            eventToPromise('keydown', page).then(() => {
              assertEquals(0, nativeLayer.getCallCount('dialogClose'));
            });
        keyEventOn(page, 'keydown', 'Escape', [], 'Escape');
        return promise;
      });

  // Tests that Cmd + Period closes the dialog only on Mac
  test(
      assert(key_event_test.TestNames.CmdPeriodClosesDialogOnMacOnly),
      function() {
        const promise = isMac ?
            nativeLayer.whenCalled('dialogClose') :
            eventToPromise('keydown', page).then(() => {
              assertEquals(0, nativeLayer.getCallCount('dialogClose'));
            });
        keyEventOn(page, 'keydown', 'Period', ['meta'], 'Period');
        return promise;
      });

  // Tests that Ctrl+Shift+P opens the system dialog.
  test(
      assert(key_event_test.TestNames.CtrlShiftPOpensSystemDialog), function() {
        let promise = null;
        if (isChromeOS) {
          // Chrome OS doesn't have a system dialog. Just make sure the key
          // event does not trigger a crash.
          promise = Promise.resolve();
        } else if (isWindows) {
          promise = nativeLayer.whenCalled('print').then((printTicket) => {
            assertTrue(JSON.parse(printTicket).showSystemDialog);
          });
        } else {
          promise = nativeLayer.whenCalled('showSystemDialog');
        }
        const modifiers = isMac ? ['meta', 'alt'] : ['ctrl', 'shift'];
        keyEventOn(page, 'keydown', 'KeyP', modifiers, 'KeyP');
        return promise;
      });
});
