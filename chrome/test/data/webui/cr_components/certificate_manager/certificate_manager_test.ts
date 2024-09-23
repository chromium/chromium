// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager shared component in the
// context of the Settings privacy page. This simplifies the test setup and
// provides better context for testing.

// clang-format off
import 'chrome://settings/strings.m.js';
import 'chrome://resources/cr_components/certificate_manager/ca_trust_edit_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_delete_confirmation_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_password_encryption_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_list.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_password_decryption_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_manager.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_subentry.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CaTrustEditDialogElement} from 'chrome://resources/cr_components/certificate_manager/ca_trust_edit_dialog.js';
import type {CertificateDeleteConfirmationDialogElement} from 'chrome://resources/cr_components/certificate_manager/certificate_delete_confirmation_dialog.js';
import type {CertificateListElement} from 'chrome://resources/cr_components/certificate_manager/certificate_list.js';
import type {CertificateManagerElement} from 'chrome://resources/cr_components/certificate_manager/certificate_manager.js';
import type { CertificateActionEventDetail} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_types.js';
import {CertificateAction, CertificateActionEvent} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_types.js';
import type {CertificatePasswordDecryptionDialogElement} from 'chrome://resources/cr_components/certificate_manager/certificate_password_decryption_dialog.js';
import type {CertificatePasswordEncryptionDialogElement} from 'chrome://resources/cr_components/certificate_manager/certificate_password_encryption_dialog.js';
import type {CertificateSubentryElement} from 'chrome://resources/cr_components/certificate_manager/certificate_subentry.js';
import type {CaTrustInfo, CertificatesBrowserProxy, CertificatesError, CertificatesOrgGroup, CertificateSubnode} from 'chrome://resources/cr_components/certificate_manager/certificates_browser_proxy.js';
import { CertificatesBrowserProxyImpl, CertificateType} from 'chrome://resources/cr_components/certificate_manager/certificates_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

/**
 * A test version of CertificatesBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
class TestCertificatesBrowserProxy extends TestBrowserProxy implements
    CertificatesBrowserProxy {
  private caTrustInfo_: CaTrustInfo;
  private certificatesError_: CertificatesError|null = null;

  constructor() {
    super([
      'deleteCertificate',
      'editCaCertificateTrust',
      'exportCertificate',
      'exportPersonalCertificate',
      'exportPersonalCertificatePasswordSelected',
      'getCaCertificateTrust',
      'importCaCertificate',
      'importCaCertificateTrustSelected',
      'importPersonalCertificate',
      'importPersonalCertificatePasswordSelected',
      'importServerCertificate',
      'refreshCertificates',
      'viewCertificate',
    ]);

    this.caTrustInfo_ = {ssl: true, email: true, objSign: true};
  }

  setCaCertificateTrust(caTrustInfo: CaTrustInfo) {
    this.caTrustInfo_ = caTrustInfo;
  }

  getCaCertificateTrust(id: string) {
    this.methodCalled('getCaCertificateTrust', id);
    return Promise.resolve(this.caTrustInfo_);
  }

  importServerCertificate() {
    this.methodCalled('importServerCertificate');
    return Promise.resolve();
  }

  importCaCertificate() {
    this.methodCalled('importCaCertificate');
    return Promise.resolve('dummyName');
  }

  importCaCertificateTrustSelected(
      ssl: boolean, email: boolean, objSign: boolean) {
    this.methodCalled(
        'importCaCertificateTrustSelected',
        {ssl: ssl, email: email, objSign: objSign});
    return this.fulfillRequest_();
  }

  editCaCertificateTrust(
      id: string, ssl: boolean, email: boolean, objSign: boolean) {
    this.methodCalled('editCaCertificateTrust', {id, ssl, email, objSign});
    return this.fulfillRequest_();
  }

  /**
   * Forces some of the browser proxy methods to start returning errors.
   */
  forceCertificatesError() {
    this.certificatesError_ = {
      title: 'DummyError',
      description: 'DummyDescription',
    };
  }

  /**
   * @return A promise that is resolved or rejected based on the
   *     value of |certificatesError_|.
   */
  private fulfillRequest_(): Promise<void> {
    return this.certificatesError_ === null ?
        Promise.resolve() :
        Promise.reject(this.certificatesError_);
  }

  deleteCertificate(id: string) {
    this.methodCalled('deleteCertificate', id);
    return this.fulfillRequest_();
  }

  exportPersonalCertificatePasswordSelected(password: string) {
    this.methodCalled('exportPersonalCertificatePasswordSelected', password);
    return this.fulfillRequest_();
  }

  importPersonalCertificate(useHardwareBacked: boolean) {
    this.methodCalled('importPersonalCertificate', useHardwareBacked);
    return Promise.resolve(true);
  }

  importPersonalCertificatePasswordSelected(password: string) {
    this.methodCalled('importPersonalCertificatePasswordSelected', password);
    return this.fulfillRequest_();
  }

  refreshCertificates() {
    this.methodCalled('refreshCertificates');
  }

  viewCertificate(id: string) {
    this.methodCalled('viewCertificate', id);
  }

  exportCertificate(id: string) {
    this.methodCalled('exportCertificate', id);
  }

  exportPersonalCertificate(id: string) {
    this.methodCalled('exportPersonalCertificate', id);
    return Promise.resolve();
  }

  cancelImportExportCertificate() {}
}

function createSampleCertificateOrgGroup(): CertificatesOrgGroup {
  return {
    id: 'dummyCertificateId',
    name: 'dummyCertificateName',
    containsPolicyCerts: false,
    subnodes: [createSampleCertificateSubnode()],
  };
}

function createSampleCertificateSubnode(): CertificateSubnode {
  return {
    extractable: false,
    id: 'dummySubnodeId',
    name: 'dummySubnodeName',
    policy: false,
    canBeDeleted: true,
    canBeEdited: true,
    untrusted: false,
    webTrustAnchor: false,
  };
}

/**
 * Triggers an 'input' event on the given text input field (which triggers
 * validation to occur for password fields being tested in this file).
 */
async function triggerInputEvent(element: CrInputElement) {
  await element.updateComplete;
  // The actual key code is irrelevant for tests.
  const kSpaceBar = 32;
  keyEventOn(element, 'input', kSpaceBar);
  await element.updateComplete;
}

suite('CaTrustEditDialogTests', function() {
  let dialog: CaTrustEditDialogElement;

  let browserProxy: TestCertificatesBrowserProxy;

  const caTrustInfo: CaTrustInfo = {ssl: true, email: false, objSign: false};

  setup(async function() {
    browserProxy = new TestCertificatesBrowserProxy();
    browserProxy.setCaCertificateTrust(caTrustInfo);

    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('ca-trust-edit-dialog');
  });

  teardown(function() {
    dialog.remove();
  });

  test('EditSuccess', async function() {
    dialog.model = createSampleCertificateSubnode();
    document.body.appendChild(dialog);

    const id = await browserProxy.whenCalled('getCaCertificateTrust');

    assertEquals((dialog.model as CertificateSubnode).id, id);
    assertEquals(caTrustInfo.ssl, dialog.$.ssl.checked);
    assertEquals(caTrustInfo.email, dialog.$.email.checked);
    assertEquals(caTrustInfo.objSign, dialog.$.objSign.checked);

    // Simulate toggling all checkboxes.
    dialog.$.ssl.click();
    dialog.$.email.click();
    dialog.$.objSign.click();

    // Simulate clicking 'OK'.
    dialog.$.ok.click();

    const {id: model_id, ssl, email, objSign} =
        await browserProxy.whenCalled('editCaCertificateTrust');

    assertEquals((dialog.model as CertificateSubnode).id, model_id);
    // Checking that the values sent to C++ are reflecting the
    // changes made by the user (toggling all checkboxes).
    assertEquals(caTrustInfo.ssl, !ssl);
    assertEquals(caTrustInfo.email, !email);
    assertEquals(caTrustInfo.objSign, !objSign);
    // Check that the dialog is closed.
    assertFalse(dialog.$.dialog.open);
  });

  test('ImportSuccess', async function() {
    dialog.model = {name: 'Dummy certificate name'};
    document.body.appendChild(dialog);

    assertFalse(dialog.$.ssl.checked);
    assertFalse(dialog.$.email.checked);
    assertFalse(dialog.$.objSign.checked);

    dialog.$.ssl.click();
    dialog.$.email.click();

    // Simulate clicking 'OK'.
    dialog.$.ok.click();
    const {ssl, email, objSign} =
        await browserProxy.whenCalled('importCaCertificateTrustSelected');

    assertTrue(ssl);
    assertTrue(email);
    assertFalse(objSign);
  });

  test('EditError', async function() {
    dialog.model = createSampleCertificateSubnode();
    document.body.appendChild(dialog);
    browserProxy.forceCertificatesError();

    const whenErrorEventFired = eventToPromise('certificates-error', dialog);

    await browserProxy.whenCalled('getCaCertificateTrust');

    dialog.$.ok.click();
    await browserProxy.whenCalled('editCaCertificateTrust');

    await whenErrorEventFired;
  });
});

suite('CertificateDeleteConfirmationDialogTests', function() {
  let dialog: CertificateDeleteConfirmationDialogElement;
  let browserProxy: TestCertificatesBrowserProxy;

  const model = createSampleCertificateSubnode();

  setup(function() {
    browserProxy = new TestCertificatesBrowserProxy();
    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('certificate-delete-confirmation-dialog');
    dialog.model = model;
    dialog.certificateType = CertificateType.PERSONAL;
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('DeleteSuccess', async function() {
    assertTrue(dialog.$.dialog.open);
    // Check that the dialog title includes the certificate name.
    const titleEl = dialog.$.dialog.querySelector('[slot=title]');
    assertTrue(!!titleEl);
    assertTrue(titleEl.textContent!.includes(model.name));

    // Simulate clicking 'OK'.
    dialog.$.ok.click();

    const id = await browserProxy.whenCalled('deleteCertificate');
    assertEquals(model.id, id);
    // Check that the dialog is closed.
    assertFalse(dialog.$.dialog.open);
  });

  test('DeleteError', async function() {
    browserProxy.forceCertificatesError();
    const whenErrorEventFired = eventToPromise('certificates-error', dialog);

    // Simulate clicking 'OK'.
    dialog.$.ok.click();
    const id = await browserProxy.whenCalled('deleteCertificate');
    assertEquals(model.id, id);
    // Ensure that the 'error' event was fired.
    await whenErrorEventFired;
  });
});

suite('CertificatePasswordEncryptionDialogTests', function() {
  let dialog: CertificatePasswordEncryptionDialogElement;
  let browserProxy: TestCertificatesBrowserProxy;

  const methodName = 'exportPersonalCertificatePasswordSelected';

  setup(function() {
    browserProxy = new TestCertificatesBrowserProxy();
    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('certificate-password-encryption-dialog');
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('EncryptSuccess', async function() {
    const passwordInputElements = dialog.$.dialog.querySelectorAll('cr-input');
    const passwordInputElement = passwordInputElements[0];
    assertTrue(!!passwordInputElement);
    const confirmPasswordInputElement = passwordInputElements[1];
    assertTrue(!!confirmPasswordInputElement);

    assertTrue(dialog.$.dialog.open);
    assertTrue(dialog.$.ok.disabled);

    // Test that the 'OK' button is disabled when the password fields are
    // empty (even though they both have the same value).
    await triggerInputEvent(passwordInputElement);
    assertTrue(dialog.$.ok.disabled);

    // Test that the 'OK' button is disabled until the two password fields
    // match.
    passwordInputElement.value = 'foopassword';
    await triggerInputEvent(passwordInputElement);
    assertTrue(dialog.$.ok.disabled);
    confirmPasswordInputElement.value = passwordInputElement.value;
    await triggerInputEvent(confirmPasswordInputElement);
    assertFalse(dialog.$.ok.disabled);

    // Simulate clicking 'OK'.
    dialog.$.ok.click();

    const password = await browserProxy.whenCalled(methodName);
    assertEquals(passwordInputElement.value, password);
    // Check that the dialog is closed.
    assertFalse(dialog.$.dialog.open);
  });

  test('EncryptError', async function() {
    browserProxy.forceCertificatesError();

    const passwordInputElements = dialog.$.dialog.querySelectorAll('cr-input');
    const passwordInputElement = passwordInputElements[0];
    assertTrue(!!passwordInputElement);
    const confirmPasswordInputElement = passwordInputElements[1];
    assertTrue(!!confirmPasswordInputElement);

    passwordInputElement.value = 'foopassword';
    confirmPasswordInputElement.value = passwordInputElement.value;
    await triggerInputEvent(passwordInputElement);

    const whenErrorEventFired = eventToPromise('certificates-error', dialog);
    dialog.$.ok.click();

    await browserProxy.whenCalled(methodName);
    await whenErrorEventFired;
  });
});

suite('CertificatePasswordDecryptionDialogTests', function() {
  let dialog: CertificatePasswordDecryptionDialogElement;
  let browserProxy: TestCertificatesBrowserProxy;

  const methodName = 'importPersonalCertificatePasswordSelected';

  setup(function() {
    browserProxy = new TestCertificatesBrowserProxy();
    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('certificate-password-decryption-dialog');
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('DecryptSuccess', async function() {
    const passwordInputElement = dialog.$.dialog.querySelector('cr-input');
    assertTrue(!!passwordInputElement);
    assertTrue(dialog.$.dialog.open);

    // Test that the 'OK' button is enabled even when the password field is
    // empty.
    assertEquals('', passwordInputElement.value);
    assertFalse(dialog.$.ok.disabled);

    passwordInputElement.value = 'foopassword';
    await passwordInputElement.updateComplete;
    assertFalse(dialog.$.ok.disabled);

    // Simulate clicking 'OK'.
    dialog.$.ok.click();

    const password = await browserProxy.whenCalled(methodName);
    assertEquals(passwordInputElement.value, password);
    // Check that the dialog is closed.
    assertFalse(dialog.$.dialog.open);
  });

  test('DecryptError', async function() {
    browserProxy.forceCertificatesError();
    // Simulate entering some password.
    const passwordInputElement = dialog.$.dialog.querySelector('cr-input');
    assertTrue(!!passwordInputElement);
    passwordInputElement.value = 'foopassword';
    await triggerInputEvent(passwordInputElement);

    const whenErrorEventFired = eventToPromise('certificates-error', dialog);
    dialog.$.ok.click();
    await browserProxy.whenCalled(methodName);
    await whenErrorEventFired;
  });
});

suite('CertificateSubentryTests', function() {
  let subentry: CertificateSubentryElement;
  let browserProxy: TestCertificatesBrowserProxy;

  /**
   * @return A promise firing once |CertificateActionEvent| fires.
   */
  function actionEventToPromise():
      Promise<CustomEvent<CertificateActionEventDetail>> {
    return eventToPromise(CertificateActionEvent, subentry);
  }

  setup(function() {
    browserProxy = new TestCertificatesBrowserProxy();
    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subentry = document.createElement('certificate-subentry');
    subentry.model = createSampleCertificateSubnode();
    subentry.certificateType = CertificateType.PERSONAL;
    document.body.appendChild(subentry);

    // Bring up the popup menu for the following tests to use.
    subentry.$.dots.click();
    flush();
  });

  teardown(function() {
    subentry.remove();
  });

  // Test case where 'View' option is tapped.
  test('MenuOptions_View', async function() {
    const viewButton = subentry.shadowRoot!.querySelector<HTMLElement>('#view');
    assertTrue(!!viewButton);
    viewButton.click();
    const id = await browserProxy.whenCalled('viewCertificate');
    assertEquals(subentry.model.id, id);
  });

  // Test that the 'Edit' option is only shown when appropriate and that
  // once tapped the correct event is fired.
  test('MenuOptions_Edit', function() {
    const editButton = subentry.shadowRoot!.querySelector<HTMLElement>('#edit');
    assertTrue(!!editButton);

    let model = createSampleCertificateSubnode();
    model.canBeEdited = false;
    subentry.model = model;
    assertTrue(editButton.hidden);

    model = createSampleCertificateSubnode();
    model.canBeEdited = true;
    subentry.model = model;
    assertFalse(editButton.hidden);

    subentry.model = createSampleCertificateSubnode();
    const waitForActionEvent = actionEventToPromise();
    editButton.click();
    return waitForActionEvent.then(function(event) {
      const detail = event.detail;
      assertEquals(CertificateAction.EDIT, detail.action);
      assertEquals(
          subentry.model.id, (detail.subnode as CertificateSubnode).id);
      assertEquals(subentry.certificateType, detail.certificateType);
    });
  });

  // Test that the 'Delete' option is only shown when appropriate and that
  // once tapped the correct event is fired.
  test('MenuOptions_Delete', function() {
    const deleteButton =
        subentry.shadowRoot!.querySelector<HTMLElement>('#delete');
    assertTrue(!!deleteButton);

    // Should be disabled when 'model.canBeDeleted' is false.
    const model = createSampleCertificateSubnode();
    model.canBeDeleted = false;
    subentry.model = model;
    assertTrue(deleteButton.hidden);

    subentry.model = createSampleCertificateSubnode();
    const waitForActionEvent = actionEventToPromise();
    deleteButton.click();
    return waitForActionEvent.then(function(event) {
      const detail = event.detail;
      assertEquals(CertificateAction.DELETE, detail.action);
      assertEquals(
          subentry.model.id, (detail.subnode as CertificateSubnode).id);
    });
  });

  // Test that the 'Export' option is always shown when the certificate type
  // is not PERSONAL and that once tapped the correct event is fired.
  test('MenuOptions_Export', async function() {
    subentry.certificateType = CertificateType.SERVER;
    const exportButton =
        subentry.shadowRoot!.querySelector<HTMLElement>('#export');
    assertTrue(!!exportButton);
    assertFalse(exportButton.hidden);
    exportButton.click();
    const id = await browserProxy.whenCalled('exportCertificate');
    assertEquals(subentry.model.id, id);
  });

  // Test case of exporting a PERSONAL certificate.
  test('MenuOptions_ExportPersonal', async function() {
    const exportButton =
        subentry.shadowRoot!.querySelector<HTMLElement>('#export');
    assertTrue(!!exportButton);

    // Should be disabled when 'model.extractable' is false.
    assertTrue(exportButton.hidden);

    const model = createSampleCertificateSubnode();
    model.extractable = true;
    subentry.model = model;
    assertFalse(exportButton.hidden);

    const waitForActionEvent = actionEventToPromise();
    exportButton.click();
    const id = await browserProxy.whenCalled('exportPersonalCertificate');

    assertEquals(subentry.model.id, id);

    // A promise firing once |CertificateActionEvent| is fired.
    const event = await waitForActionEvent;

    const detail = event.detail;
    assertEquals(CertificateAction.EXPORT_PERSONAL, detail.action);
    assertEquals(subentry.model.id, (detail.subnode as CertificateSubnode).id);
  });
});

suite('CertificateManagerTests', function() {
  let page: CertificateManagerElement;
  let browserProxy: TestCertificatesBrowserProxy;

  enum CertificateCategoryIndex {
    PERSONAL = 0,
    SERVER = 1,
    CA = 2,
    OTHER = 3,
  }

  setup(function() {
    browserProxy = new TestCertificatesBrowserProxy();
    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('certificate-manager');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  /**
   * Test that the page requests information from the browser on startup and
   * that it gets populated accordingly.
   */
  test('Initialization', async function() {
    // Trigger all category tabs to be added to the DOM.
    const crTabsElement = page.shadowRoot!.querySelector('cr-tabs');
    assertTrue(!!crTabsElement);
    crTabsElement.selected = CertificateCategoryIndex.PERSONAL;
    await crTabsElement.updateComplete;
    crTabsElement.selected = CertificateCategoryIndex.SERVER;
    await crTabsElement.updateComplete;
    crTabsElement.selected = CertificateCategoryIndex.CA;
    await crTabsElement.updateComplete;
    crTabsElement.selected = CertificateCategoryIndex.OTHER;
    await crTabsElement.updateComplete;
    const certificateLists =
        page.shadowRoot!.querySelectorAll('certificate-list');
    assertEquals(4, certificateLists.length);

    async function assertCertificateListLength(
        listIndex: CertificateCategoryIndex, expectedSize: number) {
      // Need to switch to the corresponding tab before querying the DOM.
      assertTrue(!!crTabsElement);
      crTabsElement.selected = listIndex;
      await crTabsElement.updateComplete;
      const certificateEntries =
          certificateLists[listIndex]!.shadowRoot!.querySelectorAll(
              'certificate-entry');
      assertEquals(expectedSize, certificateEntries.length);
    }

    await assertCertificateListLength(CertificateCategoryIndex.PERSONAL, 0);
    await assertCertificateListLength(CertificateCategoryIndex.SERVER, 0);
    await assertCertificateListLength(CertificateCategoryIndex.CA, 0);
    await assertCertificateListLength(CertificateCategoryIndex.OTHER, 0);

    await browserProxy.whenCalled('refreshCertificates');
    // Simulate response for personal and CA certificates.
    webUIListenerCallback(
        'certificates-changed', 'personalCerts',
        [createSampleCertificateOrgGroup()]);
    webUIListenerCallback(
        'certificates-changed', 'caCerts',
        [createSampleCertificateOrgGroup(), createSampleCertificateOrgGroup()]);
    flush();

    assertCertificateListLength(CertificateCategoryIndex.PERSONAL, 1);
    assertCertificateListLength(CertificateCategoryIndex.SERVER, 0);
    assertCertificateListLength(CertificateCategoryIndex.CA, 2);
    assertCertificateListLength(CertificateCategoryIndex.OTHER, 0);
  });

  /**
   * Tests that a dialog opens as a response to a CertificateActionEvent.
   * @param dialogTagName The type of dialog to test.
   */
  function testDialogOpensOnAction(
      dialogTagName: string,
      eventDetail: CertificateActionEventDetail): Promise<void> {
    assertFalse(!!page.shadowRoot!.querySelector(dialogTagName));
    const whenDialogOpen = eventToPromise('cr-dialog-open', page);
    page.dispatchEvent(new CustomEvent(
        CertificateActionEvent,
        {bubbles: true, composed: true, detail: eventDetail}));

    // Some dialogs are opened after some async operation to fetch initial
    // data. Ensure that the underlying cr-dialog is actually opened before
    // returning.
    return whenDialogOpen.then(() => {
      assertTrue(!!page.shadowRoot!.querySelector(dialogTagName));
    });
  }

  test('OpensDialog_DeleteConfirmation', function() {
    return testDialogOpensOnAction('certificate-delete-confirmation-dialog', {
      action: CertificateAction.DELETE,
      subnode: createSampleCertificateSubnode(),
      certificateType: CertificateType.PERSONAL,
      anchor: page,
    });
  });

  test('OpensDialog_PasswordEncryption', function() {
    return testDialogOpensOnAction('certificate-password-encryption-dialog', {
      action: CertificateAction.EXPORT_PERSONAL,
      subnode: createSampleCertificateSubnode(),
      certificateType: CertificateType.PERSONAL,
      anchor: page,
    });
  });

  test('OpensDialog_PasswordDecryption', function() {
    return testDialogOpensOnAction('certificate-password-decryption-dialog', {
      action: CertificateAction.IMPORT,
      subnode: createSampleCertificateSubnode(),
      certificateType: CertificateType.PERSONAL,
      anchor: page,
    });
  });

  test('OpensDialog_CaTrustEdit', function() {
    return testDialogOpensOnAction('ca-trust-edit-dialog', {
      action: CertificateAction.EDIT,
      subnode: createSampleCertificateSubnode(),
      certificateType: CertificateType.CA,
      anchor: page,
    });
  });

  test('OpensDialog_CaTrustImport', function() {
    return testDialogOpensOnAction('ca-trust-edit-dialog', {
      action: CertificateAction.IMPORT,
      subnode: {name: 'Dummy Certificate Name', id: ''},
      certificateType: CertificateType.CA,
      anchor: page,
    });
  });

  // <if expr="chromeos_ash">

  async function renderTabContents() {
    const crTabs = page.shadowRoot!.querySelector('cr-tabs');
    assertTrue(!!crTabs);
    crTabs.selected = CertificateCategoryIndex.PERSONAL;
    await crTabs.updateComplete;
    crTabs.selected = CertificateCategoryIndex.CA;
    await crTabs.updateComplete;
  }

  // Test that import buttons are hidden by default.
  test('ImportButton_Default', async function() {
    await renderTabContents();
    const certificateLists =
        page.shadowRoot!.querySelectorAll('certificate-list');
    const clientImportButton = certificateLists[0]!.$.import;
    assertTrue(clientImportButton.hidden);
    const clientImportAndBindButton = certificateLists[0]!.$.importAndBind;
    assertTrue(clientImportAndBindButton.hidden);
    const caImportButton = certificateLists[1]!.$.import;
    assertTrue(caImportButton.hidden);
  });

  // Test that ClientCertificateManagementAllowed policy is applied to the
  // UI when management is allowed.
  test('ImportButton_ClientPolicyAllowed', async function() {
    await renderTabContents();
    const certificateLists =
        page.shadowRoot!.querySelectorAll('certificate-list');

    await browserProxy.whenCalled('refreshCertificates');
    webUIListenerCallback(
        'client-import-allowed-changed', true /* clientImportAllowed */);
    // Verify that import buttons are shown in the client certificate
    // tab.
    const clientImportButton = certificateLists[0]!.$.import;
    assertFalse(clientImportButton.hidden);
    const clientImportAndBindButton = certificateLists[0]!.$.importAndBind;
    assertFalse(clientImportAndBindButton.hidden);
    // Verify that import button is still hidden in the CA certificate
    // tab.
    const caImportButton = certificateLists[1]!.$.import;
    assertTrue(caImportButton.hidden);
  });

  // Test that ClientCertificateManagementAllowed policy is applied to the
  // UI when management is not allowed.
  test('ImportButton_ClientPolicyDisallowed', async function() {
    await renderTabContents();
    const certificateLists =
        page.shadowRoot!.querySelectorAll('certificate-list');

    await browserProxy.whenCalled('refreshCertificates');
    webUIListenerCallback(
        'client-import-allowed-changed', false /* clientImportAllowed */);
    // Verify that import buttons are still hidden in the client
    // certificate tab.
    const clientImportButton = certificateLists[0]!.$.import;
    assertTrue(clientImportButton.hidden);
    const clientImportAndBindButton = certificateLists[0]!.$.importAndBind;
    assertTrue(clientImportAndBindButton.hidden);
    // Verify that import button is still hidden in the CA certificate
    // tab.
    const caImportButton = certificateLists[1]!.$.import;
    assertTrue(caImportButton.hidden);
  });

  // Test that CACertificateManagementAllowed policy is applied to the
  // UI when management is allowed.
  test('ImportButton_CAPolicyAllowed', async function() {
    await renderTabContents();
    const certificateLists =
        page.shadowRoot!.querySelectorAll('certificate-list');

    await browserProxy.whenCalled('refreshCertificates');
    webUIListenerCallback(
        'ca-import-allowed-changed', true /* clientImportAllowed */);
    // Verify that import buttons are still hidden in the client
    // certificate tab.
    const clientImportButton = certificateLists[0]!.$.import;
    assertTrue(clientImportButton.hidden);
    const clientImportAndBindButton = certificateLists[0]!.$.importAndBind;
    assertTrue(clientImportAndBindButton.hidden);
    // Verify that import button is shown in the CA certificate tab.
    const caImportButton = certificateLists[1]!.$.import;
    assertFalse(caImportButton.hidden);
  });

  // Test that CACertificateManagementAllowed policy is applied to the
  // UI when management is not allowed.
  test('ImportButton_CAPolicyDisallowed', async function() {
    await renderTabContents();
    const certificateLists =
        page.shadowRoot!.querySelectorAll('certificate-list');

    await browserProxy.whenCalled('refreshCertificates');
    webUIListenerCallback(
        'ca-import-allowed-changed', false /* clientImportAllowed */);
    // Verify that import buttons are still hidden in the client
    // certificate tab.
    const clientImportButton = certificateLists[0]!.$.import;
    assertTrue(clientImportButton.hidden);
    const clientImportAndBindButton = certificateLists[0]!.$.importAndBind;
    assertTrue(clientImportAndBindButton.hidden);
    // Verify that import button is still hidden in the CA certificate
    // tab.
    const caImportButton = certificateLists[1]!.$.import;
    assertTrue(caImportButton.hidden);
  });
  // </if>
});

suite('CertificateListTests', function() {
  let element: CertificateListElement;
  let browserProxy: TestCertificatesBrowserProxy;

  setup(function() {
    browserProxy = new TestCertificatesBrowserProxy();
    CertificatesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('certificate-list');
    document.body.appendChild(element);
  });

  teardown(function() {
    element.remove();
  });

  /**
   * Tests the "Import" button functionality.
   * @param proxyMethodName The name of the proxy method expected to be
   *     called.
   * @param actionEventExpected Whether a CertificateActionEvent is expected
   *     to fire as a result tapping the Import button.
   * @param bindBtn Whether to click on the import and bind btn.
   */
  async function testImportForCertificateType(
      certificateType: CertificateType, proxyMethodName: string,
      actionEventExpected: boolean, bindBtn: boolean) {
    element.certificateType = certificateType;
    flush();

    const importButton = bindBtn ?
        element.shadowRoot!.querySelector<HTMLElement>('#importAndBind') :
        element.shadowRoot!.querySelector<HTMLElement>('#import');
    assertTrue(!!importButton);

    const waitForActionEvent = actionEventExpected ?
        eventToPromise(CertificateActionEvent, element) :
        Promise.resolve(null);

    importButton.click();
    const arg = await browserProxy.whenCalled(proxyMethodName);

    if (proxyMethodName === 'importPersonalCertificate') {
      assertNotEquals(arg, undefined);
      assertEquals(arg, bindBtn);
    }
    const event = await waitForActionEvent;

    if (actionEventExpected) {
      assertEquals(CertificateAction.IMPORT, event.detail.action);
      assertEquals(certificateType, event.detail.certificateType);
    }
  }

  test('ImportButton_Personal', async function() {
    await testImportForCertificateType(
        CertificateType.PERSONAL, 'importPersonalCertificate', true, false);
  });

  // <if expr="chromeos_ash">
  test('ImportAndBindButton_Personal', async function() {
    await testImportForCertificateType(
        CertificateType.PERSONAL, 'importPersonalCertificate', true, true);
  });
  // </if>

  test('ImportButton_Server', async function() {
    await testImportForCertificateType(
        CertificateType.SERVER, 'importServerCertificate', false, false);
  });

  test('ImportButton_CA', async function() {
    await testImportForCertificateType(
        CertificateType.CA, 'importCaCertificate', true, false);
  });
});
