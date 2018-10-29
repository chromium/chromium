// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('key_event_test', function() {
  /** @enum {string} */
  const TestNames = {
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

  const suiteName = 'KeyEventTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?PrintPreviewNativeLayer} */
    let nativeLayer = null;

    /** @override */
    setup(function() {
      const initialSettings =
          print_preview_test_utils.getDefaultInitialSettings();
      nativeLayer = new print_preview.NativeLayerStub();
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      nativeLayer.setPageCount(3);
      print_preview.NativeLayer.setInstance(nativeLayer);
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview_new.PluginProxy.setInstance(pluginProxy);

      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      pluginProxy.setLoadCallback(previewArea.onPluginLoad_.bind(previewArea));

      // Wait for initialization to complete.
      return Promise
          .all([
            nativeLayer.whenCalled('getInitialSettings'),
            nativeLayer.whenCalled('getPrinterCapabilities')
          ])
          .then(function() {
            Polymer.dom.flush();
          });
    });

    // Tests that the enter key triggers a call to print.
    test(assert(TestNames.EnterTriggersPrint), function() {
      const whenPrintCalled = nativeLayer.whenCalled('print');
      MockInteractions.keyEventOn(page, 'keydown', 'Enter', [], 'Enter');
      return whenPrintCalled;
    });

    // Tests that the numpad enter key triggers a call to print.
    test(assert(TestNames.NumpadEnterTriggersPrint), function() {
      const whenPrintCalled = nativeLayer.whenCalled('print');
      MockInteractions.keyEventOn(page, 'keydown', 'NumpadEnter', [], 'Enter');
      return whenPrintCalled;
    });

    // Tests that the enter key triggers a call to print if an input is the
    // source of the event.
    test(assert(TestNames.EnterOnInputTriggersPrint), function() {
      const whenPrintCalled = nativeLayer.whenCalled('print');
      MockInteractions.keyEventOn(
          page.$$('print-preview-copies-settings')
              .$$('print-preview-number-settings-section')
              .$$('cr-input')
              .inputElement,
          'keydown', 'Enter', [], 'Enter');
      return whenPrintCalled;
    });

    // Tests that the enter key does not trigger a call to print if the event
    // comes from a dropdown.
    test(assert(TestNames.EnterOnDropdownDoesNotPrint), function() {
      const whenKeyEventFired = test_util.eventToPromise('keydown', page);
      MockInteractions.keyEventOn(
          page.$$('print-preview-layout-settings').$$('.md-select'), 'keydown',
          'Enter', [], 'Enter');
      return whenKeyEventFired.then(
          () => assertEquals(0, nativeLayer.getCallCount('print')));
    });

    // Tests that the enter key does not trigger a call to print if the event
    // comes from a button.
    test(assert(TestNames.EnterOnButtonDoesNotPrint), function() {
      const whenKeyEventFired = test_util.eventToPromise('keydown', page);
      MockInteractions.keyEventOn(
          page.$$('print-preview-destination-settings').$$('paper-button'),
          'keydown', 'Enter', [], 'Enter');
      return whenKeyEventFired.then(
          () => assertEquals(0, nativeLayer.getCallCount('print')));
    });

    // Tests that the enter key does not trigger a call to print if the event
    // comes from a checkbox.
    test(assert(TestNames.EnterOnCheckboxDoesNotPrint), function() {
      const moreSettingsElement = page.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
      const whenKeyEventFired = test_util.eventToPromise('keydown', page);
      MockInteractions.keyEventOn(
          page.$$('print-preview-other-options-settings').$$('cr-checkbox'),
          'keydown', 'Enter', [], 'Enter');
      return whenKeyEventFired.then(
          () => assertEquals(0, nativeLayer.getCallCount('print')));
    });

    // Tests that escape closes the dialog only on Mac.
    test(assert(TestNames.EscapeClosesDialogOnMacOnly), function() {
      const promise = cr.isMac ?
          nativeLayer.whenCalled('dialogClose') :
          test_util.eventToPromise('keydown', page).then(() => {
            assertEquals(0, nativeLayer.getCallCount('dialogClose'));
          });
      MockInteractions.keyEventOn(page, 'keydown', 'Escape', [], 'Escape');
      return promise;
    });

    // Tests that Cmd + Period closes the dialog only on Mac
    test(assert(TestNames.CmdPeriodClosesDialogOnMacOnly), function() {
      const promise = cr.isMac ?
          nativeLayer.whenCalled('dialogClose') :
          test_util.eventToPromise('keydown', page).then(() => {
            assertEquals(0, nativeLayer.getCallCount('dialogClose'));
          });
      MockInteractions.keyEventOn(
          page, 'keydown', 'Period', ['meta'], 'Period');
      return promise;
    });

    // Tests that Ctrl+Shift+P opens the system dialog.
    test(assert(TestNames.CtrlShiftPOpensSystemDialog), function() {
      let promise = null;
      if (cr.isChromeOS) {
        // Chrome OS doesn't have a system dialog. Just make sure the key event
        // does not trigger a crash.
        promise = Promise.resolve();
      } else if (cr.isWindows) {
        promise = nativeLayer.whenCalled('print').then((printTicket) => {
          assertTrue(JSON.parse(printTicket).showSystemDialog);
        });
      } else {
        promise = nativeLayer.whenCalled('showSystemDialog');
      }
      const modifiers = cr.isMac ? ['meta', 'alt'] : ['ctrl', 'shift'];
      MockInteractions.keyEventOn(page, 'keydown', 'KeyP', modifiers, 'KeyP');
      return promise;
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
