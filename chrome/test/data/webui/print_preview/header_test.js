// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, Error, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {fakeDataBind} from 'chrome://test/test_util.m.js';

window.header_test = {};
header_test.suiteName = 'HeaderTest';
/** @enum {string} */
header_test.TestNames = {
  HeaderPrinterTypes: 'header printer types',
  HeaderWithDuplex: 'header with duplex',
  HeaderWithCopies: 'header with copies',
  HeaderWithNup: 'header with nup',
  HeaderChangesForState: 'header changes for state',
  EnterprisePolicy: 'enterprise policy',
};

suite(header_test.suiteName, function() {
  /** @type {?PrintPreviewHeaderElement} */
  let header = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    header = document.createElement('print-preview-header');
    header.settings = model.settings;
    model.set('settings.duplex.available', true);
    model.set('settings.duplex.value', false);

    header.destination = new Destination(
        'FooDevice', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        'FooName', DestinationConnectionStatus.ONLINE);
    header.state = State.READY;
    header.managed = false;
    fakeDataBind(model, header, 'settings');
    document.body.appendChild(header);
  });

  function setPdfDestination() {
    header.set(
        'destination',
        new Destination(
            Destination.GooglePromotedId.SAVE_AS_PDF, DestinationType.LOCAL,
            DestinationOrigin.LOCAL, loadTimeData.getString('printToPDF'),
            DestinationConnectionStatus.ONLINE));
  }

  // Tests that the 4 different messages (non-virtual printer singular and
  // plural, virtual printer singular and plural) all show up as expected.
  test(assert(header_test.TestNames.HeaderPrinterTypes), function() {
    const summary = header.$$('.summary');
    assertEquals('1 sheet of paper', summary.textContent.trim());
    header.setSetting('pages', [1, 2, 3]);
    assertEquals('3 sheets of paper', summary.textContent.trim());
    setPdfDestination();
    assertEquals('3 pages', summary.textContent.trim());
    header.setSetting('pages', [1]);
    assertEquals('1 page', summary.textContent.trim());
    // Verify the chrome://print case of a zero length document does not show
    // the summary.
    header.setSetting('pages', []);
    assertEquals('', summary.textContent);
  });

  // Tests that the message is correctly adjusted with a duplex printer.
  test(assert(header_test.TestNames.HeaderWithDuplex), function() {
    const summary = header.$$('.summary');
    assertEquals('1 sheet of paper', summary.textContent.trim());
    header.setSetting('pages', [1, 2, 3]);
    assertEquals('3 sheets of paper', summary.textContent.trim());
    header.setSetting('duplex', true);
    assertEquals('2 sheets of paper', summary.textContent.trim());
    header.setSetting('pages', [1, 2]);
    assertEquals('1 sheet of paper', summary.textContent.trim());
  });

  // Tests that the message is correctly adjusted with multiple copies.
  test(assert(header_test.TestNames.HeaderWithCopies), function() {
    const summary = header.$$('.summary');
    assertEquals('1 sheet of paper', summary.textContent.trim());
    header.setSetting('copies', 4);
    assertEquals('4 sheets of paper', summary.textContent.trim());
    header.setSetting('duplex', true);
    assertEquals('4 sheets of paper', summary.textContent.trim());
    header.setSetting('pages', [1, 2]);
    assertEquals('4 sheets of paper', summary.textContent.trim());
    header.setSetting('duplex', false);
    assertEquals('8 sheets of paper', summary.textContent.trim());
  });

  // Tests that the correct message is shown for non-READY states, and that
  // the print button is disabled appropriately.
  test(assert(header_test.TestNames.HeaderChangesForState), function() {
    const summary = header.$$('.summary');
    assertEquals('1 sheet of paper', summary.textContent.trim());

    header.state = State.NOT_READY;
    assertEquals('', summary.textContent.trim());

    header.state = State.PRINTING;
    assertEquals(
        loadTimeData.getString('printing'), summary.textContent.trim());
    setPdfDestination();
    assertEquals(loadTimeData.getString('saving'), summary.textContent.trim());

    header.state = State.ERROR;
    assertEquals('', summary.textContent.trim());

    const testError = 'Error printing to cloud print';
    header.cloudPrintErrorMessage = testError;
    header.error = Error.CLOUD_PRINT_ERROR;
    header.state = State.FATAL_ERROR;
    assertEquals(testError, summary.textContent.trim());
  });

  // Tests that enterprise badge shows up if any setting is managed.
  test(assert(header_test.TestNames.EnterprisePolicy), function() {
    assertTrue(header.$$('iron-icon').hidden);
    header.managed = true;
    assertFalse(header.$$('iron-icon').hidden);
  });
});
