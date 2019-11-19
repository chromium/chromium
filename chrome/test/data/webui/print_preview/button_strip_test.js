// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isWindows} from 'chrome://resources/js/cr.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.button_strip_test = {};
button_strip_test.suiteName = 'ButtonStripTest';
/** @enum {string} */
button_strip_test.TestNames = {
  ButtonStripChangesForState: 'button strip changes for state',
  ButtonOrder: 'button order',
  ButtonStripFiresEvents: 'button strip fires events',
};

suite(button_strip_test.suiteName, function() {
  /** @type {?PrintPreviewButtonStripElement} */
  let buttonStrip = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    buttonStrip = document.createElement('print-preview-button-strip');

    buttonStrip.destination = new Destination(
        'FooDevice', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        'FooName', DestinationConnectionStatus.ONLINE);
    buttonStrip.state = State.READY;
    document.body.appendChild(buttonStrip);
  });

  // Tests that the correct message is shown for non-READY states, and that
  // the print button is disabled appropriately.
  test(
      assert(button_strip_test.TestNames.ButtonStripChangesForState),
      function() {
        const printButton = buttonStrip.$$('.action-button');
        assertFalse(printButton.disabled);

        buttonStrip.state = State.NOT_READY;
        assertTrue(printButton.disabled);

        buttonStrip.state = State.PRINTING;
        assertTrue(printButton.disabled);

        buttonStrip.state = State.ERROR;
        assertTrue(printButton.disabled);

        buttonStrip.state = State.FATAL_ERROR;
        assertTrue(printButton.disabled);
      });

  // Tests that the buttons are in the correct order for different platforms.
  // See https://crbug.com/880562.
  test(assert(button_strip_test.TestNames.ButtonOrder), function() {
    // Verify that there are only 2 buttons.
    assertEquals(
        2, buttonStrip.shadowRoot.querySelectorAll('cr-button').length);

    const firstButton = buttonStrip.$$('cr-button:not(:last-child)');
    const lastButton = buttonStrip.$$('cr-button:last-child');
    const printButton = buttonStrip.$$('cr-button.action-button');
    const cancelButton = buttonStrip.$$('cr-button.cancel-button');

    if (isWindows) {
      // On Windows, the print button is on the left.
      assertEquals(firstButton, printButton);
      assertEquals(lastButton, cancelButton);
    } else {
      assertEquals(firstButton, cancelButton);
      assertEquals(lastButton, printButton);
    }
  });

  // Tests that the button strip fires print-requested and cancel-requested
  // events.
  test(assert(button_strip_test.TestNames.ButtonStripFiresEvents), function() {
    const printButton = buttonStrip.$$('cr-button.action-button');
    const cancelButton = buttonStrip.$$('cr-button.cancel-button');

    const whenPrintRequested = eventToPromise('print-requested', buttonStrip);
    printButton.click();
    return whenPrintRequested.then(() => {
      const whenCancelRequested =
          eventToPromise('cancel-requested', buttonStrip);
      cancelButton.click();
      return whenCancelRequested;
    });
  });
});
