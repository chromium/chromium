// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {SummaryPanelElement} from 'chrome://os-print/js/summary_panel.js';
import {SHEETS_USED_CHANGED_EVENT, SummaryPanelController} from 'chrome://os-print/js/summary_panel_controller.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

suite('SummaryPanel', () => {
  const sheetsUsedSelector = '#sheetsUsed';

  let element: SummaryPanelElement|null = null;
  let controller: SummaryPanelController|null = null;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element =
        document.createElement(SummaryPanelElement.is) as SummaryPanelElement;
    assertTrue(!!element);
    document.body.append(element);
    assert(element);
    controller = element.getControllerForTesting();
    assert(controller);
    await updateSheetsUsed(/*sheetsUsed=*/ 1);

    // CrOS components are async and require flushTasks before they are
    // available.
    await flushTasks();
  });

  teardown(() => {
    if (element) {
      element.remove();
    }
    element = null;
    controller = null;
  });

  // Sets sheets used in controller and wait for UI to update.
  async function updateSheetsUsed(sheetsUsed: number): Promise<void> {
    assert(controller);
    const sheetsUsedEvent =
        eventToPromise(SHEETS_USED_CHANGED_EVENT, controller);
    controller.setSheetsUsedForTesting(sheetsUsed);
    await sheetsUsedEvent;
    await flushTasks();
  }

  // Verify the summary-panel element can be rendered, contains print, cancel,
  // and sheets used elements.
  test('element renders', async () => {
    assert(element);
    assertTrue(isVisible(element));

    const cancelButtonSelector = '#cancel';
    assertTrue(
        isChildVisible(element, cancelButtonSelector),
        `Should display ${cancelButtonSelector}`);
    const printButtonSelector = '#print';
    assertTrue(
        isChildVisible(element, printButtonSelector),
        `Should display ${printButtonSelector}`);
    assertTrue(
        isChildVisible(element, sheetsUsedSelector),
        `Should display ${sheetsUsedSelector}`);
  });

  // Verify summary-panel element has a controller configured.
  test('has element controller', async () => {
    assertTrue(
        !!controller,
        `${SummaryPanelElement.is} should have controller configured`);
  });

  // Verify #sheetsUsed updates to the string defined by SummaryPanelController
  // when a `sheets_used_changed` event occurs.
  test('sheets used matches controller getSheetsUsed', async () => {
    assert(element);
    assert(controller);
    const sheetsUsed = strictQuery<HTMLSpanElement>(
        sheetsUsedSelector, element.shadowRoot, HTMLSpanElement);

    updateSheetsUsed(/*sheetsUsed=*/ 0);
    const expectedInitialText = '';
    assertEquals(
        expectedInitialText, sheetsUsed.innerText,
        `${sheetsUsedSelector} text should match ${expectedInitialText}`);
    assertEquals(
        expectedInitialText, controller.getSheetsUsedText(),
        `${SummaryPanelElement.is} controller text should match ${
            expectedInitialText}`);

    // Controller emits `sheets-used-changed`
    updateSheetsUsed(/*sheetsUsed=*/ 1);

    const expectedUpdatedText = '1 used';
    assertEquals(
        expectedUpdatedText, sheetsUsed.innerText,
        `${sheetsUsedSelector} text should match ${expectedUpdatedText}`);
    assertEquals(
        expectedUpdatedText, controller.getSheetsUsedText(),
        `${SummaryPanelElement.is} controller text should match ${
            expectedUpdatedText}`);
  });
});
