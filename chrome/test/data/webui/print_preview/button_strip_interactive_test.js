// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, PrintPreviewButtonStripElement, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

window.button_strip_interactive_test = {};
const button_strip_interactive_test = window.button_strip_interactive_test;
button_strip_interactive_test.suiteName = 'ButtonStripInteractiveTest';
/** @enum {string} */
button_strip_interactive_test.TestNames = {
  FocusPrintOnReady: 'focus print on ready',
};

suite(button_strip_interactive_test.suiteName, function() {
  /** @type {!PrintPreviewButtonStripElement} */
  let buttonStrip;

  /** @override */
  setup(function() {
    document.body.innerHTML = '';
    buttonStrip = /** @type {!PrintPreviewButtonStripElement} */ (
        document.createElement('print-preview-button-strip'));
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
        const printButton =
            assert(buttonStrip.shadowRoot.querySelector('.action-button'));
        const whenFocusDone = eventToPromise('focus', printButton);

        // Simulate initialization finishing.
        buttonStrip.state = State.READY;
        return whenFocusDone;
      });
});
