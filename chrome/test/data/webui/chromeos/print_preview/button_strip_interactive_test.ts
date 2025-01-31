// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewButtonStripElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('ButtonStripInteractiveTest', function() {
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
  test('focus print on ready', function() {
    const printButton = buttonStrip.shadowRoot!.querySelector('.action-button');
    assert(printButton);
    const whenFocusDone = eventToPromise('focus', printButton);

    // Simulate initialization finishing.
    buttonStrip.state = State.READY;
    return whenFocusDone;
  });
});
