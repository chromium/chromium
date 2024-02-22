// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {SummaryPanelElement} from 'chrome://os-print/js/summary_panel.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

suite('SummaryPanel', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  // Verify the summary-panel element can be rendered, contains print, cancel,
  // and sheets used elements.
  test('element renders', async () => {
    const element =
        document.createElement(SummaryPanelElement.is) as SummaryPanelElement;
    assertTrue(!!element);
    document.body.append(element);
    assertTrue(isVisible(element));

    // CrOS components are async and require flushTasks before they are
    // available.
    await flushTasks();
    const cancelButtonSelector = '#cancel';
    assertTrue(
        isChildVisible(element, cancelButtonSelector),
        `Should display ${cancelButtonSelector}`);
    const printButtonSelector = '#print';
    assertTrue(
        isChildVisible(element, printButtonSelector),
        `Should display ${printButtonSelector}`);
    const sheetsUsedSelector = '#sheetsUsed';
    assertTrue(
        isChildVisible(element, sheetsUsedSelector),
        `Should display ${sheetsUsedSelector}`);
  });
});
