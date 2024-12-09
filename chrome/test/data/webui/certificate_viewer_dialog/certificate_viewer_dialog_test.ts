// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrTreeBaseElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_base.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {CertViewerBrowserProxyImpl} from 'chrome://view-cert/browser_proxy.js';
import type {CertMetadataChangeResult, CertViewerBrowserProxy} from 'chrome://view-cert/browser_proxy.js';
import type {TreeItemDetail} from 'chrome://view-cert/certificate_viewer.js';
import {CertificateTrust} from 'chrome://view-cert/certificate_viewer.js';
import type {ConstraintListElement} from 'chrome://view-cert/constraint_list.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
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


export class TestCertViewerBrowserProxy extends TestBrowserProxy implements
    CertViewerBrowserProxy {
  constructor() {
    super([
      'updateTrustState',
    ]);
  }

  updateTrustState(newTrust: number): Promise<CertMetadataChangeResult> {
    this.methodCalled('updateTrustState', newTrust);
    return sendWithPromise('updateTrustState', newTrust);
  }
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

    // Test that when cert metadata is not provided is, modifications tab is
    // hidden.
    const modificationsTab = getRequiredElement('modifications-tab');
    assertTrue(modificationsTab.hidden);
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

  test('CheckMetadata', async function() {
    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);

    const trustStateSelector =
        (getRequiredElement('trust-state-select') as HTMLSelectElement);
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_UNSPECIFIED,
        Number(trustStateSelector.value) as CertificateTrust);
    assertTrue(trustStateSelector.disabled);

    const constraintList =
        (getRequiredElement('constraints') as ConstraintListElement);
    assertEquals(3, constraintList.constraints.length);
    assertTrue(constraintList.constraints.includes('*.example.com'));
    assertTrue(constraintList.constraints.includes('*.domainname.com'));
    assertTrue(constraintList.constraints.includes('127.0.0.1/24'));
  });

  test('EditTrustState', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);

    const trustStateSelector =
        (getRequiredElement('trust-state-select') as HTMLSelectElement);
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_UNSPECIFIED,
        Number(trustStateSelector.value) as CertificateTrust);
    assertFalse(trustStateSelector.disabled);
    const trustStateErrorMessage =
        (getRequiredElement('trust-state-select-error') as HTMLElement);
    assertTrue(trustStateErrorMessage.classList.contains('hide-error'));

    trustStateSelector.value =
        (CertificateTrust.CERTIFICATE_TRUST_TRUSTED as number).toString();
    // Changing the value with javascript doesn't trigger the change event;
    // trigger the event manually.
    trustStateSelector.dispatchEvent(new Event('change'));
    await testBrowserProxy.whenCalled('updateTrustState');

    const changeFinished = trustStateSelector.disabled ?
        eventToPromise(
            'trust-state-change-finished-for-testing', document.body) :
        Promise.resolve();
    await changeFinished;

    assertTrue(trustStateErrorMessage.classList.contains('hide-error'));
  });


  test('EditTrustStateError', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);

    const trustStateSelector =
        (getRequiredElement('trust-state-select') as HTMLSelectElement);
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_UNSPECIFIED,
        Number(trustStateSelector.value) as CertificateTrust);
    assertFalse(trustStateSelector.disabled);
    const trustStateErrorMessage =
        (getRequiredElement('trust-state-select-error') as HTMLElement);
    assertTrue(trustStateErrorMessage.classList.contains('hide-error'));

    trustStateSelector.value =
        (CertificateTrust.CERTIFICATE_TRUST_TRUSTED as number).toString();
    // Changing the value with javascript doesn't trigger the change event;
    // trigger the event manually.
    trustStateSelector.dispatchEvent(new Event('change'));
    await testBrowserProxy.whenCalled('updateTrustState');

    const changeFinished = trustStateSelector.disabled ?
        eventToPromise(
            'trust-state-change-finished-for-testing', document.body) :
        Promise.resolve();
    await changeFinished;

    assertFalse(trustStateErrorMessage.classList.contains('hide-error'));
    assertEquals(
        trustStateErrorMessage.innerText,
        'There was an error saving the trust state change');
  });
});
