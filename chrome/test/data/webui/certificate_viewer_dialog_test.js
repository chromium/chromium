// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

/**
 * Find the first tree item (in the certificate fields tree) with a value.
 * @param {!Element} tree Certificate fields subtree to search.
 * @return {?Element} The first found element with a value, null if not found.
 */
function getElementWithValue(tree) {
  for (var i = 0; i < tree.childNodes.length; i++) {
    var element = tree.childNodes[i];
    if (element.detail && element.detail.payload &&
        element.detail.payload.val) {
      return element;
    }
    if (element = getElementWithValue(element)) {
      return element;
    }
  }
  return null;
}

suite('CertificateViewer', function() {
  // Tests that the dialog opened to the correct URL.
  test('DialogURL', function() {
    assertEquals(chrome.getVariableValue('expectedUrl'), window.location.href);
  });

  // Tests for the correct common name in the test certificate.
  test('CommonName', function() {
    assertEquals('www.google.com', $('issued-cn').textContent);
  });

  test('Details', async function() {
    var certHierarchy = $('hierarchy');
    var certFields = $('cert-fields');
    var certFieldVal = $('cert-field-value');

    // Select the second tab, causing its data to be loaded if needed.
    $('tabbox').selectedIndex = 1;

    // There must be at least one certificate in the hierarchy.
    assertLT(0, certHierarchy.childNodes.length);

    // Wait for the '-for-testing' event to fire only if |certFields| is not
    // populated yet, otherwise don't wait, the event has already fired.
    const whenLoaded = certFields.childNodes.length === 0 ?
        eventToPromise(
            'certificate-fields-updated-for-testing', document.body) :
        Promise.resolve();

    // Select the first certificate on the chain and ensure the details show up.
    await whenLoaded;
    assertLT(0, certFields.childNodes.length);

    // Test that a field can be selected to see the details for that
    // field.
    var item = getElementWithValue(certFields);
    assertNotEquals(null, item);
    certFields.selectedItem = item;
    assertEquals(item.detail.payload.val, certFieldVal.textContent);

    // Test that selecting an item without a value empties the field.
    certFields.selectedItem = certFields.childNodes[0];
    assertEquals('', certFieldVal.textContent);
  });
});
