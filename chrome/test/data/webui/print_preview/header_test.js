// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, Error, PrintPreviewPluralStringProxyImpl, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {TestPluralStringProxy} from '../test_plural_string_proxy.js';
import {fakeDataBind} from '../test_util.m.js';

window.header_test = {};
const header_test = window.header_test;
header_test.suiteName = 'HeaderTest';
/** @enum {string} */
header_test.TestNames = {
  HeaderPrinterTypes: 'header printer types',
  HeaderChangesForState: 'header changes for state',
  EnterprisePolicy: 'enterprise policy',
};

suite(header_test.suiteName, function() {
  /** @type {!PrintPreviewHeaderElement} */
  let header;

  /** @type {TestPluralStringProxy} */
  let pluralString = null;

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    pluralString = new TestPluralStringProxy();
    PrintPreviewPluralStringProxyImpl.instance_ = pluralString;
    pluralString.text = '1 sheet of paper';

    const model = /** @type {!PrintPreviewModelElement} */ (
        document.createElement('print-preview-model'));
    document.body.appendChild(model);

    header = /** @type {!PrintPreviewHeaderElement} */ (
        document.createElement('print-preview-header'));
    header.settings = model.settings;
    model.set('settings.duplex.available', true);
    model.set('settings.duplex.value', false);

    header.destination = new Destination(
        'FooDevice', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        'FooName', DestinationConnectionStatus.ONLINE);
    header.state = State.READY;
    header.managed = false;
    header.sheetCount = 1;
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
  test(assert(header_test.TestNames.HeaderPrinterTypes), async function() {
    const summary = header.$$('.summary');
    {
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      assertEquals('1 sheet of paper', summary.textContent.trim());
      assertEquals('printPreviewSheetSummaryLabel', messageName);
      assertEquals(1, itemCount);
    }
    {
      pluralString.resetResolver('getPluralString');
      header.sheetCount = 3;
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      assertEquals('printPreviewSheetSummaryLabel', messageName);
      assertEquals(3, itemCount);
    }
    {
      pluralString.resetResolver('getPluralString');
      setPdfDestination();
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      assertEquals('printPreviewPageSummaryLabel', messageName);
      assertEquals(3, itemCount);
    }
    {
      pluralString.resetResolver('getPluralString');
      header.sheetCount = 1;
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      assertEquals('printPreviewPageSummaryLabel', messageName);
      assertEquals(1, itemCount);
    }
    // Verify the chrome://print case of a zero length document does not show
    // the summary.
    header.sheetCount = 0;
    assertEquals('', summary.textContent);
  });

  // Tests that the correct message is shown for non-READY states, and that
  // the print button is disabled appropriately.
  test(assert(header_test.TestNames.HeaderChangesForState), async function() {
    const summary = header.$$('.summary');
    await pluralString.whenCalled('getPluralString');
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
