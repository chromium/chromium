// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewHeaderElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, GooglePromotedDestinationId, PrintPreviewPluralStringProxyImpl, State} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HeaderTest', function() {
  let header: PrintPreviewHeaderElement;

  let pluralString: TestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    pluralString = new TestPluralStringProxy();
    PrintPreviewPluralStringProxyImpl.setInstance(pluralString);
    pluralString.text = '1 sheet of paper';

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    header = document.createElement('print-preview-header');
    model.set('settings.duplex.available', true);
    model.set('settings.duplex.value', false);

    header.destination = new Destination(
        'FooDevice', DestinationOrigin.EXTENSION, 'FooName',
        {extensionId: 'aaa111', extensionName: 'myPrinterExtension'});
    header.state = State.READY;
    header.managed = false;
    header.sheetCount = 1;
    document.body.appendChild(header);
  });

  function setPdfDestination() {
    header.destination = new Destination(
        GooglePromotedDestinationId.SAVE_AS_PDF, DestinationOrigin.LOCAL,
        loadTimeData.getString('printToPDF'));
  }

  // Tests that the 4 different messages (non-virtual printer singular and
  // plural, virtual printer singular and plural) all show up as expected.
  test('HeaderPrinterTypes', async function() {
    const summary = header.shadowRoot.querySelector('.summary')!;
    {
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      await microtasksFinished();
      assertEquals('1 sheet of paper', summary.textContent!.trim());
      assertEquals('printPreviewSheetSummaryLabel', messageName);
      assertEquals(1, itemCount);
    }
    {
      pluralString.resetResolver('getPluralString');
      header.sheetCount = 3;
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      await microtasksFinished();
      assertEquals('printPreviewSheetSummaryLabel', messageName);
      assertEquals(3, itemCount);
    }
    {
      pluralString.resetResolver('getPluralString');
      setPdfDestination();
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      await microtasksFinished();
      assertEquals('printPreviewPageSummaryLabel', messageName);
      assertEquals(3, itemCount);
    }
    {
      pluralString.resetResolver('getPluralString');
      header.sheetCount = 1;
      const {messageName, itemCount} =
          await pluralString.whenCalled('getPluralString');
      await microtasksFinished();
      assertEquals('printPreviewPageSummaryLabel', messageName);
      assertEquals(1, itemCount);
    }
    // Verify the chrome://print case of a zero length document does not show
    // the summary.
    header.sheetCount = 0;
    await microtasksFinished();
    assertEquals('', summary.textContent);
  });

  // Tests that the correct message is shown for non-READY states, and that
  // the print button is disabled appropriately.
  test('HeaderChangesForState', async function() {
    const summary = header.shadowRoot.querySelector('.summary')!;
    await pluralString.whenCalled('getPluralString');
    await microtasksFinished();
    assertEquals('1 sheet of paper', summary.textContent!.trim());

    header.state = State.NOT_READY;
    await microtasksFinished();
    assertEquals('', summary.textContent!.trim());

    header.state = State.PRINTING;
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('printing'), summary.textContent!.trim());
    setPdfDestination();
    await microtasksFinished();
    assertEquals(loadTimeData.getString('saving'), summary.textContent!.trim());

    header.state = State.ERROR;
    await microtasksFinished();
    assertEquals('', summary.textContent!.trim());
  });

  // Tests that enterprise badge shows up if any setting is managed.
  test('EnterprisePolicy', async function() {
    assertTrue(header.shadowRoot.querySelector('cr-icon')!.hidden);
    header.managed = true;
    await microtasksFinished();
    assertFalse(header.shadowRoot.querySelector('cr-icon')!.hidden);
  });
});
