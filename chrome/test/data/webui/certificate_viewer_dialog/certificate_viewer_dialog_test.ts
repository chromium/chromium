// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrTreeBaseElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_base.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {TreeItemDetail} from 'chrome://view-cert/certificate_viewer.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/**
 * Find the first tree item (in the certificate fields tree) with a value.
 * @param tree Certificate fields subtree to search.
 * @return The first found element with a value, null if not found.
 */
function getElementWithValue(tree: CrTreeBaseElement): CrTreeBaseElement|null {
  for (let i = 0; i < tree.items.length; i++) {
    let element: CrTreeBaseElement|null = tree.items[i]!;
    const detail = element.detail as TreeItemDetail|null;
    if (detail && detail.payload && detail.payload.val) {
      return element;
    }
    element = getElementWithValue(element);
    if (element) {
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
    assertTrue(getRequiredElement('general-error').hidden);
    assertFalse(getRequiredElement('general-fields').hidden);

    assertEquals(
        'www.google.com', getRequiredElement('issued-cn').textContent);
  });

  test('Details', async function() {
    const certHierarchy = getRequiredElement<CrTreeBaseElement>('hierarchy');
    const certFields = getRequiredElement<CrTreeBaseElement>('cert-fields');
    const certFieldVal = getRequiredElement('cert-field-value');

    // Select the second tab, causing its data to be loaded if needed.
    getRequiredElement('tabbox').setAttribute('selected-index', '1');

    // There must be at least one certificate in the hierarchy.
    assertLT(0, certHierarchy.items.length);

    // Wait for the '-for-testing' event to fire only if |certFields| is not
    // populated yet, otherwise don't wait, the event has already fired.
    const whenLoaded = certFields.items.length === 0 ?
        eventToPromise(
            'certificate-fields-updated-for-testing', document.body) :
        Promise.resolve();

    // Select the first certificate on the chain and ensure the details show up.
    await whenLoaded;
    assertLT(0, certFields.items.length);

    // Test that a field can be selected to see the details for that
    // field.
    const item = getElementWithValue(certFields);
    assertTrue(!!item);
    certFields.selectedItem = item;
    assertEquals((item.detail as TreeItemDetail).payload.val, certFieldVal.textContent);

    // Test that selecting an item without a value empties the field.
    certFields.selectedItem = certFields.items[0]!;
    assertEquals('', certFieldVal.textContent);
  });

  test('InvalidCert', async function() {
    // Error should be shown instead of cert fields.
    assertFalse(getRequiredElement('general-error').hidden);
    assertTrue(getRequiredElement('general-fields').hidden);

    // Cert hash should still be shown.
    assertEquals(
        '787188ffa5cca48212ed291e62cb03e1' +
            '1c1f8279df07feb1d2b0e02e0e4aa9e4',
        getRequiredElement('sha256').textContent);
  });
});
