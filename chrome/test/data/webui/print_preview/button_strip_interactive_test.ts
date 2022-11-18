// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationOrigin, PrintPreviewButtonStripElement, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const button_strip_interactive_test = {
  suiteName: 'ButtonStripInteractiveTest',
  TestNames: {
    FocusPrintOnReady: 'focus print on ready',
  },
};

Object.assign(
    window, {button_strip_interactive_test: button_strip_interactive_test});

suite(button_strip_interactive_test.suiteName, function() {
  let buttonStrip: PrintPreviewButtonStripElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    buttonStrip = document.createElement('print-preview-button-strip');
    buttonStrip.destination = new Destination(
        'FooDevice',
        DestinationOrigin.EXTENSION,
        'FooName',
        {extensionId: 'aaa111', extensionName: 'myPrinterExtension'},
    );
    buttonStrip.state = State.NOT_READY;
    buttonStrip.firstLoad = true;
    document.body.appendChild(buttonStrip);
  });

  // Tests that the print button is automatically focused when the destination
  // is ready.
  test(button_strip_interactive_test.TestNames.FocusPrintOnReady, function() {
    const printButton = buttonStrip.shadowRoot!.querySelector('.action-button');
    assert(printButton);
    const whenFocusDone = eventToPromise('focus', printButton);

    // Simulate initialization finishing.
    buttonStrip.state = State.READY;
    return whenFocusDone;
  });
});
