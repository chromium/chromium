// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.button_strip_interactive_test = {};
button_strip_interactive_test.suiteName = 'ButtonStripInteractiveTest';
/** @enum {string} */
button_strip_interactive_test.TestNames = {
  FocusPrintOnReady: 'focus print on ready',
};

suite(button_strip_interactive_test.suiteName, function() {
  /** @type {?PrintPreviewButtonStripElement} */
  let buttonStrip = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    buttonStrip = document.createElement('print-preview-button-strip');
    buttonStrip.destination = new Destination(
        'FooDevice', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        'FooName', DestinationConnectionStatus.ONLINE);
    buttonStrip.state = State.NOT_READY;
    buttonStrip.firstLoad = true;
    document.body.appendChild(buttonStrip);
  });

  // Tests that the print button is automatically focused when the destination
  // is ready.
  test(
      assert(button_strip_interactive_test.TestNames.FocusPrintOnReady),
      function() {
        const printButton = buttonStrip.$$('.action-button');
        assertTrue(!!printButton);
        const whenFocusDone = eventToPromise('focus', printButton);

        // Simulate initialization finishing.
        buttonStrip.state = State.READY;
        return whenFocusDone;
      });
});
