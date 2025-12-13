// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewHeaderElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, GooglePromotedDestinationId, PrintPreviewPluralStringProxyImpl, State} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HeaderTest', function() {
  let header: PrintPreviewHeaderElement;
  let model: PrintPreviewModelElement;

  let pluralStringProxy: TestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    pluralStringProxy = new TestPluralStringProxy();
    PrintPreviewPluralStringProxyImpl.setInstance(pluralStringProxy);
    pluralStringProxy.text = '1 sheet of paper';

    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    header = document.createElement('print-preview-header');
    model.setSettingAvailableForTesting('duplex', true);
    model.setSetting('duplex', false);

    header.destination = new Destination(
        'FooDevice', DestinationOrigin.EXTENSION, 'FooName',
        {extensionId: 'aaa111', extensionName: 'myPrinterExtension'});
    header.state = State.READY;
    header.managed = false;
    document.body.appendChild(header);
  });

  function setPdfDestination() {
    header.destination = new Destination(
        GooglePromotedDestinationId.SAVE_AS_PDF, DestinationOrigin.LOCAL,
        loadTimeData.getString('printToPDF'));
  }

  async function assertGetPluralStringCall(
      expectedCount: number, expectedStringId: string) {
    const {messageName, itemCount} =
        await pluralStringProxy.whenCalled('getPluralString');
    assertEquals(expectedCount, itemCount);
    assertEquals(expectedStringId, messageName);
    return microtasksFinished();
  }

  // Tests that the 4 different messages (non-virtual printer singular and
  // plural, virtual printer singular and plural) all show up as expected.
  test('HeaderPrinterTypes', async function() {
    const summary = header.shadowRoot.querySelector('.summary');
    assertTrue(!!summary);

    await assertGetPluralStringCall(1, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse1';
    model.setSetting('pages', [1, 2, 3]);
    await assertGetPluralStringCall(3, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse2';
    setPdfDestination();
    await assertGetPluralStringCall(3, 'printPreviewPageSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse3';
    model.setSetting('pages', [1]);
    await assertGetPluralStringCall(1, 'printPreviewPageSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    // Verify the chrome://print case of a zero length document does not show
    // the summary.
    model.setSetting('pages', []);
    await microtasksFinished();
    assertEquals('', summary.textContent);
  });

  // Tests that the correct message is shown for non-READY states, and that
  // the print button is disabled appropriately.
  test('HeaderChangesForState', async function() {
    const summary = header.shadowRoot.querySelector('.summary')!;
    await pluralStringProxy.whenCalled('getPluralString');
    await microtasksFinished();
    assertEquals('1 sheet of paper', summary.textContent.trim());

    header.state = State.NOT_READY;
    await microtasksFinished();
    assertEquals('', summary.textContent.trim());

    header.state = State.PRINTING;
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('printing'), summary.textContent.trim());
    setPdfDestination();
    await microtasksFinished();
    assertEquals(loadTimeData.getString('saving'), summary.textContent.trim());

    header.state = State.ERROR;
    await microtasksFinished();
    assertEquals('', summary.textContent.trim());
  });

  // Tests that enterprise badge shows up if any setting is managed.
  test('EnterprisePolicy', async function() {
    assertTrue(header.shadowRoot.querySelector('cr-icon')!.hidden);
    header.managed = true;
    await microtasksFinished();
    assertFalse(header.shadowRoot.querySelector('cr-icon')!.hidden);
  });

  // Tests that number of sheets is correctly calculated if duplex setting is
  // enabled.
  test('SheetCountWithDuplex', async function() {
    const summary = header.shadowRoot.querySelector('.summary');
    assertTrue(!!summary);

    assertFalse(model.getSettingValue('duplex'));
    assertEquals(1, model.getSettingValue('copies'));

    await assertGetPluralStringCall(1, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse1';
    model.setSetting('pages', [1, 2, 3]);
    await assertGetPluralStringCall(3, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse2';
    model.setSetting('duplex', true);
    await assertGetPluralStringCall(2, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse3';
    model.setSetting('pages', [1, 2]);
    await assertGetPluralStringCall(1, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());
  });

  // Tests that number of sheets is correctly calculated if multiple copies
  // setting is enabled.
  test('SheetCountWithCopies', async function() {
    const summary = header.shadowRoot.querySelector('.summary');
    assertTrue(!!summary);

    assertFalse(model.getSettingValue('duplex'));
    assertEquals(1, model.getSettingValue('copies'));

    await assertGetPluralStringCall(1, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse1';
    model.setSetting('copies', 4);
    await assertGetPluralStringCall(4, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    model.setSetting('duplex', true);
    await microtasksFinished();
    // No backend call expected, since the sheetCount did not change.
    assertEquals(0, pluralStringProxy.getCallCount('getPluralString'));
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    model.setSetting('pages', [1, 2]);
    await microtasksFinished();
    // No backend call expected, since the sheetCount did not change.
    assertEquals(0, pluralStringProxy.getCallCount('getPluralString'));
    assertEquals(pluralStringProxy.text, summary.textContent.trim());

    pluralStringProxy.resetResolver('getPluralString');
    pluralStringProxy.text = 'dummyResponse2';
    model.setSetting('duplex', false);
    await assertGetPluralStringCall(8, 'printPreviewSheetSummaryLabel');
    assertEquals(pluralStringProxy.text, summary.textContent.trim());
  });
});
