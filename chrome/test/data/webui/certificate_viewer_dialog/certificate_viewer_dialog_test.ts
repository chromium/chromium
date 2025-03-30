// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrTreeBaseElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_base.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {CertViewerBrowserProxyImpl} from 'chrome://view-cert/browser_proxy.js';
import type {CertMetadataChangeResult, CertViewerBrowserProxy, ConstraintChangeResult} from 'chrome://view-cert/browser_proxy.js';
import type {TreeItemDetail} from 'chrome://view-cert/certificate_viewer.js';
import {CertificateTrust} from 'chrome://view-cert/certificate_viewer.js';
import type {ModificationsPanelElement} from 'chrome://view-cert/modifications_panel.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
      'addConstraint',
      'deleteConstraint',
    ]);
  }

  private trustStateChangeResult: CertMetadataChangeResult = {
    success: true,
  };

  private constraintChangeResult: ConstraintChangeResult = {
    status: {
      success: true,
    },
  };

  setTrustStateChangeResult(newChangeResult: CertMetadataChangeResult) {
    this.trustStateChangeResult = newChangeResult;
  }

  setConstraintChangeResult(newChangeResult: ConstraintChangeResult) {
    this.constraintChangeResult = newChangeResult;
  }

  updateTrustState(newTrust: number): Promise<CertMetadataChangeResult> {
    this.methodCalled('updateTrustState', newTrust);
    return Promise.resolve(this.trustStateChangeResult);
  }

  addConstraint(constraint: string): Promise<ConstraintChangeResult> {
    this.methodCalled('addConstraint', constraint);
    return Promise.resolve(this.constraintChangeResult);
  }

  deleteConstraint(constraint: string): Promise<ConstraintChangeResult> {
    this.methodCalled('deleteConstraint', constraint);
    return Promise.resolve(this.constraintChangeResult);
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

    // Test that when cert metadata is not provided, modifications tab and
    // panel are not present.
    assertFalse(!!document.querySelector('#modifications-tab'));
    assertFalse(!!document.querySelector('#modifications'));
  });

  test('InvalidCert', function() {
    // Error should be shown instead of cert fields.
    assertFalse(getRequiredElement('general-error').hidden);
    assertTrue(getRequiredElement('general-fields').hidden);

    // Cert hash should still be shown.
    assertEquals(
        '787188ffa5cca48212ed291e62cb03e1' +
            '1c1f8279df07feb1d2b0e02e0e4aa9e4',
        getRequiredElement('sha256').textContent);
  });

  // Check for the default constraints that are set up in C++ for the
  // certificate.
  function checkDefaultConstraints(
      modificationsPanel: ModificationsPanelElement) {
    assertEquals(3, modificationsPanel.constraints.length);
    assertTrue(modificationsPanel.constraints.includes('example.com'));
    assertTrue(modificationsPanel.constraints.includes('domainname.com'));
    assertTrue(modificationsPanel.constraints.includes('127.0.0.1/24'));
  }

  test('CheckMetadataNotEditable', function() {
    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    // Select the modifications tab to allow for visibility checks
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_TRUSTED,
        Number(modificationsPanel.$.trustStateSelect.value) as
            CertificateTrust);
    assertTrue(modificationsPanel.$.trustStateSelect.disabled);

    checkDefaultConstraints(modificationsPanel);
    assertFalse(isVisible(modificationsPanel.$.addConstraintSection));
    assertTrue(isVisible(modificationsPanel.$.constraintListSection));
    const deleteButton =
        modificationsPanel.shadowRoot.querySelector<CrIconButtonElement>(
            '#constraint-delete-0');
    assert(deleteButton);
    assertFalse(isVisible(deleteButton));
  });

  test('CheckMetadataNotEditableNoConstraints', function() {
    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    // Select the modifications tab to allow for visibility checks
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_TRUSTED,
        Number(modificationsPanel.$.trustStateSelect.value) as
            CertificateTrust);
    assertTrue(modificationsPanel.$.trustStateSelect.disabled);

    assertFalse(isVisible(modificationsPanel.$.constraintListSection));
    assertFalse(isVisible(modificationsPanel.$.addConstraintSection));
  });

  test('CheckMetadataEditable', function() {
    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    // Select the modifications tab to allow for visibility checks
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_TRUSTED,
        Number(modificationsPanel.$.trustStateSelect.value) as
            CertificateTrust);
    assertFalse(modificationsPanel.$.trustStateSelect.disabled);

    checkDefaultConstraints(modificationsPanel);
    assertTrue(isVisible(modificationsPanel.$.addConstraintSection));
    assertFalse(modificationsPanel.$.addConstraintButton.disabled);
    assertTrue(isVisible(modificationsPanel.$.constraintListSection));
    const deleteButton =
        modificationsPanel.shadowRoot.querySelector<CrIconButtonElement>(
            '#constraint-delete-0');
    assert(deleteButton);
    assertTrue(isVisible(deleteButton));
    assertFalse(deleteButton.disabled);
  });

  test('CheckMetadataEditableNoConstraints', function() {
    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    // Select the modifications tab to allow for visibility checks
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_TRUSTED,
        Number(modificationsPanel.$.trustStateSelect.value) as
            CertificateTrust);
    assertFalse(modificationsPanel.$.trustStateSelect.disabled);

    // Can add constraints
    assertTrue(isVisible(modificationsPanel.$.addConstraintSection));
    assertFalse(modificationsPanel.$.addConstraintButton.disabled);
    // No constraints to see, hide the list.
    assertEquals(0, modificationsPanel.constraints.length);
    assertFalse(isVisible(modificationsPanel.$.constraintListSection));
  });

  test('EditTrustState', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    // Select the modifications tab to allow for visibility checks
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_TRUSTED,
        Number(modificationsPanel.$.trustStateSelect.value) as
            CertificateTrust);
    assertFalse(modificationsPanel.$.trustStateSelect.disabled);
    assertFalse(isVisible(modificationsPanel.$.trustStateSelectError));

    modificationsPanel.$.trustStateSelect.value =
        (CertificateTrust.CERTIFICATE_TRUST_UNSPECIFIED as number).toString();
    // Changing the value with javascript doesn't trigger the change event;
    // trigger the event manually.
    modificationsPanel.$.trustStateSelect.dispatchEvent(new Event('change'));
    await testBrowserProxy.whenCalled('updateTrustState');
    await microtasksFinished();

    assertFalse(isVisible(modificationsPanel.$.trustStateSelectError));
    assertEquals(
        modificationsPanel.trustStateValue,
        (CertificateTrust.CERTIFICATE_TRUST_UNSPECIFIED as number).toString());
    // Changing trust to unspecified should remove visibility of constraints
    // sections.
    assertFalse(
        isVisible(modificationsPanel.$.constraintListSection), 'visible 1');
    assertFalse(
        isVisible(modificationsPanel.$.addConstraintSection), 'visible 2');
  });

  test('EditTrustStateError', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setTrustStateChangeResult({
      success: false,
      errorMessage: 'error message',
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    // Select the modifications tab to allow for visibility checks
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    assertEquals(
        CertificateTrust.CERTIFICATE_TRUST_TRUSTED,
        Number(modificationsPanel.$.trustStateSelect.value) as
            CertificateTrust);
    assertFalse(modificationsPanel.$.trustStateSelect.disabled);
    assertTrue(isVisible(modificationsPanel), 'modifications tab not visible');
    assertTrue(
        isVisible(modificationsPanel.$.trustStateSelect),
        'trust state selector not visible');
    assertFalse(isVisible(modificationsPanel.$.trustStateSelectError));

    modificationsPanel.$.trustStateSelect.value =
        (CertificateTrust.CERTIFICATE_TRUST_UNSPECIFIED as number).toString();
    // Changing the value with javascript doesn't trigger the change event;
    // trigger the event manually.
    modificationsPanel.$.trustStateSelect.dispatchEvent(new Event('change'));
    await testBrowserProxy.whenCalled('updateTrustState');
    await microtasksFinished();

    assertTrue(isVisible(modificationsPanel.$.trustStateSelectError));
    assertEquals(
        modificationsPanel.$.trustStateSelectError.innerText.trim(),
        'error message');
    assertEquals(
        modificationsPanel.trustStateValue,
        (CertificateTrust.CERTIFICATE_TRUST_TRUSTED as number).toString());
    assertEquals(
        modificationsPanel.$.trustStateSelect.value,
        (CertificateTrust.CERTIFICATE_TRUST_TRUSTED as number).toString());
    // Trust didn't change, so constraints sections should still be visible.
    assertTrue(
        isVisible(modificationsPanel.$.constraintListSection), 'visible 1');
    assertTrue(
        isVisible(modificationsPanel.$.addConstraintSection), 'visible 2');
  });

  test('AddConstraintDNS', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setConstraintChangeResult({
      constraints: ['example.com', 'foo.com', 'domainname.com', '127.0.0.1/24'],
      status: {success: true},
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    checkDefaultConstraints(modificationsPanel);

    modificationsPanel.$.addConstraintInput.value = 'foo.com';
    modificationsPanel.$.addConstraintButton.click();
    await testBrowserProxy.whenCalled('addConstraint');
    await microtasksFinished();

    assertEquals(4, modificationsPanel.constraints.length);
    assertTrue(modificationsPanel.constraints.includes('example.com'));
    assertTrue(modificationsPanel.constraints.includes('foo.com'));
    assertTrue(modificationsPanel.constraints.includes('domainname.com'));
    assertTrue(modificationsPanel.constraints.includes('127.0.0.1/24'));
    assertFalse(modificationsPanel.$.addConstraintInput.invalid);
    assertEquals(modificationsPanel.$.addConstraintInput.value, '');
  });

  test('AddConstraintCIDR', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setConstraintChangeResult({
      constraints:
          ['example.com', 'domainname.com', '127.0.0.1/24', '10.10.0.0/15'],
      status: {success: true},
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    checkDefaultConstraints(modificationsPanel);

    modificationsPanel.$.addConstraintInput.value = '10.10.0.0/15';
    modificationsPanel.$.addConstraintButton.click();
    await testBrowserProxy.whenCalled('addConstraint');
    await microtasksFinished();

    assertEquals(4, modificationsPanel.constraints.length);
    assertTrue(modificationsPanel.constraints.includes('example.com'));
    assertTrue(modificationsPanel.constraints.includes('domainname.com'));
    assertTrue(modificationsPanel.constraints.includes('127.0.0.1/24'));
    assertTrue(modificationsPanel.constraints.includes('10.10.0.0/15'));
    assertFalse(modificationsPanel.$.addConstraintInput.invalid);
    assertEquals(modificationsPanel.$.addConstraintInput.value, '');
  });

  test('AddConstraintError', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setConstraintChangeResult({
      status: {
        success: false,
        errorMessage: 'error message',
      },
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    checkDefaultConstraints(modificationsPanel);

    modificationsPanel.$.addConstraintInput.value = 'foo.com';
    modificationsPanel.$.addConstraintButton.click();
    await testBrowserProxy.whenCalled('addConstraint');
    await microtasksFinished();

    // Constraints should not have changed.
    checkDefaultConstraints(modificationsPanel);
    // Error message should be set on the input
    assertTrue(modificationsPanel.$.addConstraintInput.invalid);
    assertEquals(
        modificationsPanel.$.addConstraintInput.errorMessage, 'error message');
    assertEquals(modificationsPanel.$.addConstraintInput.value, 'foo.com');
  });

  test('DeleteConstraintDNS', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setConstraintChangeResult({
      constraints: ['domainname.com', '127.0.0.1/24'],
      status: {success: true},
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    checkDefaultConstraints(modificationsPanel);

    const deleteButton =
        modificationsPanel.shadowRoot.querySelector<CrIconButtonElement>(
            '#constraint-delete-0');
    assert(deleteButton);
    const deleteConstraint =
        (deleteButton.parentElement as HTMLElement).innerText.trim();
    assert(
        deleteConstraint === 'example.com',
        'not deleting right element, trying to delete: ' + deleteConstraint);
    deleteButton.click();
    await testBrowserProxy.whenCalled('deleteConstraint');
    await microtasksFinished();

    assertEquals(
        2, modificationsPanel.constraints.length,
        'too long: ' + modificationsPanel.constraints.length);
    assertTrue(
        modificationsPanel.constraints.includes('domainname.com'),
        'missing domainname.com');
    assertTrue(
        modificationsPanel.constraints.includes('127.0.0.1/24'),
        'missing cidr');
    assertFalse(isVisible(modificationsPanel.$.constraintDeleteError));
  });

  test('DeleteConstraintCIDR', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setConstraintChangeResult({
      constraints: ['example.com', 'domainname.com'],
      status: {success: true},
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    checkDefaultConstraints(modificationsPanel);

    const deleteButton =
        modificationsPanel.shadowRoot.querySelector<CrIconButtonElement>(
            '#constraint-delete-2');
    assert(deleteButton);
    const deleteConstraint =
        (deleteButton.parentElement as HTMLElement).innerText.trim();
    assert(
        deleteConstraint === '127.0.0.1/24',
        'not deleting right element, trying to delete: ' + deleteConstraint);
    deleteButton.click();
    await testBrowserProxy.whenCalled('deleteConstraint');
    await microtasksFinished();

    assertEquals(2, modificationsPanel.constraints.length);
    assertTrue(modificationsPanel.constraints.includes('example.com'));
    assertTrue(modificationsPanel.constraints.includes('domainname.com'));
    assertFalse(isVisible(modificationsPanel.$.constraintDeleteError));
  });

  test('DeleteConstraintError', async function() {
    const testBrowserProxy = new TestCertViewerBrowserProxy();
    CertViewerBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setConstraintChangeResult({
      status: {
        success: false,
        errorMessage: 'error message',
      },
    });

    const modificationsTab = getRequiredElement('modifications-tab');
    assertFalse(modificationsTab.hidden);
    getRequiredElement('tabbox').setAttribute('selected-index', '2');

    const modificationsPanel =
        getRequiredElement<ModificationsPanelElement>('modifications-panel');
    checkDefaultConstraints(modificationsPanel);
    assertFalse(isVisible(modificationsPanel.$.constraintDeleteError));

    const deleteButton =
        modificationsPanel.shadowRoot.querySelector<CrIconButtonElement>(
            '#constraint-delete-0');
    assert(deleteButton);
    const deleteConstraint =
        (deleteButton.parentElement as HTMLElement).innerText.trim();
    assert(
        deleteConstraint === 'example.com',
        'not deleting right element, trying to delete: ' + deleteConstraint);
    deleteButton.click();
    await testBrowserProxy.whenCalled('deleteConstraint');
    await microtasksFinished();

    // Constraints should not have changed.
    checkDefaultConstraints(modificationsPanel);
    assertTrue(
        isVisible(modificationsPanel.$.constraintDeleteError), 'not visible');
    assertEquals(
        modificationsPanel.$.constraintDeleteError.innerText.trim(),
        'error message', 'error message not equal');
  });
});
