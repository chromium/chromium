// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Suite of tests verifying the PDF viewer as served by Print Preview's data
 * source works as expected. */

import 'chrome://print/pdf/pdf_print_wrapper.js';

import type {PdfViewerPrintElement} from 'chrome://print/pdf/pdf_print_wrapper.js';
import {pdfCreateOutOfProcessPlugin} from 'chrome://print/pdf/pdf_scripting_api.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('PdfViewerTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('Basic', async () => {
    const plugin = pdfCreateOutOfProcessPlugin(
        'chrome-untrusted://print/test.pdf',
        'chrome://print/pdf/index_print.html');

    const loaded = eventToPromise('load', plugin);
    document.body.appendChild(plugin);
    await loaded;
    assertTrue(!!plugin.contentDocument);
    const viewer: PdfViewerPrintElement|null =
        plugin.contentDocument.querySelector('pdf-viewer-print');
    assertTrue(!!viewer);
    assertTrue(plugin.contentDocument.documentElement.hasAttribute(
        'is-print-preview'));

    function verifyElement(id: string) {
      const element = viewer!.shadowRoot!.querySelector(`viewer-${id}`);
      assertTrue(!!element);
      if (id === 'zoom-toolbar') {
        assertEquals('zoomToolbar', element.id);
      } else if (id === 'page-indicator') {
        assertEquals('pageIndicator', element.id);
      } else {
        assertEquals(id, element.id);
      }
    }

    ['zoom-toolbar', 'page-indicator'].forEach(id => verifyElement(id));

    // Should also have the sizer and content divs
    assertTrue(!!viewer.shadowRoot!.querySelector('#sizer'));
    assertTrue(!!viewer.shadowRoot!.querySelector('#content'));

    // These elements don't exist in Print Preview's viewer.
    ['viewer-pdf-toolbar', 'viewer-form-warning'].forEach(
        name => assertFalse(!!viewer.shadowRoot!.querySelector(name)));

    // The error dialog only appears when it is needed.
    assertFalse(!!viewer.shadowRoot!.querySelector('viewer-error-dialog'));
    viewer.showErrorDialog = true;
    await waitAfterNextRender(viewer);
    assertTrue(!!viewer.shadowRoot!.querySelector('viewer-error-dialog'));
  });

  test('PageIndicator', async () => {
    const indicator = document.createElement('viewer-page-indicator');
    document.body.appendChild(indicator);

    // Assumes label is index + 1 if no labels are provided
    indicator.index = 2;
    await microtasksFinished();
    assertEquals('3', indicator.$.text.textContent);

    // If labels are provided, uses the index to get the label.
    indicator.pageLabels = [1, 3, 5];
    await microtasksFinished();
    assertEquals('5', indicator.$.text.textContent);
  });
});
