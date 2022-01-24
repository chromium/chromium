// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Suite of tests verifying the PDF viewer as served by Print Preview's data
 * source works as expected. */

import 'chrome://print/pdf/elements/viewer-page-indicator.js';

import {PDFCreateOutOfProcessPlugin} from 'chrome://print/pdf/pdf_scripting_api.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise, waitAfterNextRender} from '../test_util.js';

window.pdf_viewer_test = {};
pdf_viewer_test.suiteName = 'PdfViewerTest';
/** @enum {string} */
pdf_viewer_test.TestNames = {
  Basic: 'basic',
  PageIndicator: 'page indicator',
};

suite(pdf_viewer_test.suiteName, function() {
  setup(function() {
    document.body.innerHTML = '';
  });

  test(assert(pdf_viewer_test.TestNames.Basic), async () => {
    const plugin = PDFCreateOutOfProcessPlugin(
        'chrome-untrusted://print/test.pdf', 'chrome://print/pdf');

    const loaded = eventToPromise('load', plugin);
    document.body.appendChild(plugin);
    await loaded;
    const viewer = plugin.contentDocument.querySelector('pdf-viewer-pp');
    assertTrue(!!viewer);
    assertTrue(plugin.contentDocument.documentElement.hasAttribute(
        'is-print-preview'));

    const verifyElement = id => {
      const element = viewer.shadowRoot.querySelector(`viewer-${id}`);
      assertTrue(!!element);
      assertEquals(id, element.id);
    };

    ['zoom-toolbar', 'page-indicator'].forEach(id => verifyElement(id));

    // Should also have the sizer and content divs
    assertTrue(!!viewer.shadowRoot.querySelector('#sizer'));
    assertTrue(!!viewer.shadowRoot.querySelector('#content'));

    // These elements don't exist in Print Preview's viewer.
    ['viewer-pdf-toolbar', 'viewer-form-warning'].forEach(
        name => assertFalse(!!viewer.shadowRoot.querySelector(name)));

    // The error dialog only appears when it is needed.
    assertFalse(!!viewer.shadowRoot.querySelector('viewer-error-dialog'));
    viewer.showErrorDialog = true;
    await waitAfterNextRender(viewer);
    assertTrue(!!viewer.shadowRoot.querySelector('viewer-error-dialog'));
  });

  test(assert(pdf_viewer_test.TestNames.PageIndicator), function() {
    const indicator = document.createElement('viewer-page-indicator');
    document.body.appendChild(indicator);

    // Assumes label is index + 1 if no labels are provided
    indicator.index = 2;
    assertEquals('3', indicator.label);

    // If labels are provided, uses the index to get the label.
    indicator.pageLabels = ['1', '3', '5'];
    assertEquals('5', indicator.label);
  });
});
