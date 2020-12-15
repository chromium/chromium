// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager shared component in the
// context of the Settings privacy page. This simplifies the test setup and
// provides better context for testing.

// clang-format off
import 'chrome://settings/strings.m.js';
import 'chrome://resources/cr_components/certificate_manager/ca_trust_edit_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_delete_confirmation_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_list.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_manager.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_password_decryption_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_password_encryption_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_subentry.js';

import {CertificateAction, CertificateActionEvent, CertificateActionEventDetail} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_types.js';
import { CaTrustInfo,CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificatesError, CertificatesOrgGroup, CertificateSubnode, CertificateType} from 'chrome://resources/cr_components/certificate_manager/certificates_browser_proxy.js';
import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';
import {eventToPromise} from '../test_util.m.js';
// clang-format on

  /**
   * A test version of CertificatesBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @implements {CertificatesBrowserProxy}
   */
  class TestCertificatesBrowserProxy extends TestBrowserProxy {
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

      /** @private {!CaTrustInfo} */
      this.caTrustInfo_ = {ssl: true, email: true, objSign: true};

      /** @private {?CertificatesError} */
      this.certificatesError_ = null;
    }

    /** @param {!CaTrustInfo} caTrustInfo */
    setCaCertificateTrust(caTrustInfo) {
      this.caTrustInfo_ = caTrustInfo;
    }

    /** @override */
    getCaCertificateTrust(id) {
      this.methodCalled('getCaCertificateTrust', id);
      return Promise.resolve(this.caTrustInfo_);
    }

    /** @override */
    importServerCertificate() {
      this.methodCalled('importServerCertificate');
      return Promise.resolve();
    }

    /** @override */
    importCaCertificate() {
      this.methodCalled('importCaCertificate');
      return Promise.resolve('dummyName');
    }

    /** @override */
    importCaCertificateTrustSelected(ssl, email, objSign) {
      this.methodCalled(
          'importCaCertificateTrustSelected',
          {ssl: ssl, email: email, objSign: objSign});
      return this.fulfillRequest_();
    }

    /** @override */
    editCaCertificateTrust(id, ssl, email, objSign) {
      this.methodCalled(
          'editCaCertificateTrust',
          {id: id, ssl: ssl, email: email, objSign: objSign});
      return this.fulfillRequest_();
    }

    /**
     * Forces some of the browser proxy methods to start returning errors.
     */
    forceCertificatesError() {
      this.certificatesError_ = /** @type {!CertificatesError} */ (
          {title: 'DummyError', description: 'DummyDescription'});
    }

    /**
     * @return {!Promise} A promise that is resolved or rejected based on the
     * value of |certificatesError_|.
     * @private
     */
    fulfillRequest_() {
      return this.certificatesError_ === null ?
          Promise.resolve() :
          Promise.reject(this.certificatesError_);
    }

    /** @override */
    deleteCertificate(id) {
      this.methodCalled('deleteCertificate', id);
      return this.fulfillRequest_();
    }

    /** @override */
    exportPersonalCertificatePasswordSelected(password) {
      this.methodCalled('exportPersonalCertificatePasswordSelected', password);
      return this.fulfillRequest_();
    }

    /** @override */
    importPersonalCertificate(useHardwareBacked) {
      this.methodCalled('importPersonalCertificate', useHardwareBacked);
      return Promise.resolve(true);
    }

    /** @override */
    importPersonalCertificatePasswordSelected(password) {
      this.methodCalled('importPersonalCertificatePasswordSelected', password);
      return this.fulfillRequest_();
    }

    /** @override */
    refreshCertificates() {
      this.methodCalled('refreshCertificates');
    }

    /** @override */
    viewCertificate(id) {
      this.methodCalled('viewCertificate', id);
    }

    /** @override */
    exportCertificate(id) {
      this.methodCalled('exportCertificate', id);
    }

    /** @override */
    exportPersonalCertificate(id) {
      this.methodCalled('exportPersonalCertificate', id);
      return Promise.resolve();
    }

    /** @override */
    cancelImportExportCertificate() {}
  }

  /** @return {!CertificatesOrgGroup} */
  function createSampleCertificateOrgGroup() {
    return {
      id: 'dummyCertificateId',
      name: 'dummyCertificateName',
      containsPolicyCerts: false,
      subnodes: [createSampleCertificateSubnode()],
    };
  }

  /** @return {!CertificateSubnode} */
  function createSampleCertificateSubnode() {
    return {
      extractable: false,
      id: 'dummySubnodeId',
      name: 'dummySubnodeName',
      policy: false,
      canBeDeleted: true,
      canBeEdited: true,
      untrusted: false,
      urlLocked: false,
      webTrustAnchor: false,
    };
  }

  /**
   * Triggers an 'input' event on the given text input field (which triggers
   * validation to occur for password fields being tested in this file).
   * @param {!HTMLElement} element
   */
  function triggerInputEvent(element) {
    // The actual key code is irrelevant for tests.
    const kSpaceBar = 32;
    keyEventOn(element, 'input', kSpaceBar);
  }

  suite('CaTrustEditDialogTests', function() {
    /** @type {?CaTrustEditDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @type {!CaTrustInfo} */
    const caTrustInfo = {ssl: true, email: false, objSign: false};

    setup(async function() {
      browserProxy = new TestCertificatesBrowserProxy();
      browserProxy.setCaCertificateTrust(caTrustInfo);

      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      dialog = /** @type {!CaTrustEditDialogElement} */ (
          document.createElement('ca-trust-edit-dialog'));
    });

    teardown(function() {
      dialog.remove();
    });

    test('EditSuccess', function() {
      dialog.model = createSampleCertificateSubnode();
      document.body.appendChild(dialog);

      return browserProxy.whenCalled('getCaCertificateTrust')
          .then(function(id) {
            assertEquals(dialog.model.id, id);
            assertEquals(caTrustInfo.ssl, dialog.$$('#ssl').checked);
            assertEquals(caTrustInfo.email, dialog.$$('#email').checked);
            assertEquals(caTrustInfo.objSign, dialog.$$('#objSign').checked);

            // Simulate toggling all checkboxes.
            dialog.$$('#ssl').click();
            dialog.$$('#email').click();
            dialog.$$('#objSign').click();

            // Simulate clicking 'OK'.
            dialog.$$('#ok').click();

            return browserProxy.whenCalled('editCaCertificateTrust');
          })
          .then(function(args) {
            assertEquals(dialog.model.id, args.id);
            // Checking that the values sent to C++ are reflecting the
            // changes made by the user (toggling all checkboxes).
            assertEquals(caTrustInfo.ssl, !args.ssl);
            assertEquals(caTrustInfo.email, !args.email);
            assertEquals(caTrustInfo.objSign, !args.objSign);
            // Check that the dialog is closed.
            assertFalse(dialog.$$('#dialog').open);
          });
    });

    test('ImportSuccess', function() {
      dialog.model = {name: 'Dummy certificate name'};
      document.body.appendChild(dialog);

      assertFalse(dialog.$$('#ssl').checked);
      assertFalse(dialog.$$('#email').checked);
      assertFalse(dialog.$$('#objSign').checked);

      dialog.$$('#ssl').click();
      dialog.$$('#email').click();

      // Simulate clicking 'OK'.
      dialog.$$('#ok').click();
      return browserProxy.whenCalled('importCaCertificateTrustSelected')
          .then(function(args) {
            assertTrue(args.ssl);
            assertTrue(args.email);
            assertFalse(args.objSign);
          });
    });

    test('EditError', function() {
      dialog.model = createSampleCertificateSubnode();
      document.body.appendChild(dialog);
      browserProxy.forceCertificatesError();

      const whenErrorEventFired = eventToPromise('certificates-error', dialog);

      return browserProxy.whenCalled('getCaCertificateTrust')
          .then(function() {
            dialog.$$('#ok').click();
            return browserProxy.whenCalled('editCaCertificateTrust');
          })
          .then(function() {
            return whenErrorEventFired;
          });
    });
  });

  suite('CertificateDeleteConfirmationDialogTests', function() {
    /** @type {!CertificateDeleteConfirmationDialogElement} */
    let dialog;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @type {!CertificateSubnode} */
    const model = createSampleCertificateSubnode();

    setup(function() {
      browserProxy = new TestCertificatesBrowserProxy();
      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      dialog = /** @type {!CertificateDeleteConfirmationDialogElement} */ (
          document.createElement('certificate-delete-confirmation-dialog'));
      dialog.model = model;
      dialog.certificateType = CertificateType.PERSONAL;
      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('DeleteSuccess', function() {
      assertTrue(dialog.$$('#dialog').open);
      // Check that the dialog title includes the certificate name.
      const titleEl = dialog.$$('#dialog').querySelector('[slot=title]');
      assertTrue(titleEl.textContent.includes(model.name));

      // Simulate clicking 'OK'.
      dialog.$$('#ok').click();

      return browserProxy.whenCalled('deleteCertificate').then(function(id) {
        assertEquals(model.id, id);
        // Check that the dialog is closed.
        assertFalse(dialog.$$('#dialog').open);
      });
    });

    test('DeleteError', function() {
      browserProxy.forceCertificatesError();
      const whenErrorEventFired = eventToPromise('certificates-error', dialog);

      // Simulate clicking 'OK'.
      dialog.$$('#ok').click();
      return browserProxy.whenCalled('deleteCertificate').then(function(id) {
        assertEquals(model.id, id);
        // Ensure that the 'error' event was fired.
        return whenErrorEventFired;
      });
    });
  });

  suite('CertificatePasswordEncryptionDialogTests', function() {
    /** @type {?CertificatePasswordEncryptionDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @type {!CertificateSubnode} */
    const model = createSampleCertificateSubnode();

    const methodName = 'exportPersonalCertificatePasswordSelected';

    setup(function() {
      browserProxy = new TestCertificatesBrowserProxy();
      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      dialog = /** @type {!CertificatePasswordEncryptionDialogElement} */ (
          document.createElement('certificate-password-encryption-dialog'));
      dialog.model = model;
      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('EncryptSuccess', function() {
      const passwordInputElements =
          dialog.$$('#dialog').querySelectorAll('cr-input');
      const passwordInputElement =
          /** @type {!HTMLElement} */ (passwordInputElements[0]);
      const confirmPasswordInputElement =
          /** @type {!HTMLElement} */ (passwordInputElements[1]);

      assertTrue(dialog.$$('#dialog').open);
      assertTrue(dialog.$$('#ok').disabled);

      // Test that the 'OK' button is disabled when the password fields are
      // empty (even though they both have the same value).
      triggerInputEvent(passwordInputElement);
      assertTrue(dialog.$$('#ok').disabled);

      // Test that the 'OK' button is disabled until the two password fields
      // match.
      passwordInputElement.value = 'foopassword';
      triggerInputEvent(passwordInputElement);
      assertTrue(dialog.$$('#ok').disabled);
      confirmPasswordInputElement.value = passwordInputElement.value;
      triggerInputEvent(confirmPasswordInputElement);
      assertFalse(dialog.$$('#ok').disabled);

      // Simulate clicking 'OK'.
      dialog.$$('#ok').click();

      return browserProxy.whenCalled(methodName).then(function(password) {
        assertEquals(passwordInputElement.value, password);
        // Check that the dialog is closed.
        assertFalse(dialog.$$('#dialog').open);
      });
    });

    test('EncryptError', function() {
      browserProxy.forceCertificatesError();

      const passwordInputElements =
          dialog.$$('#dialog').querySelectorAll('cr-input');
      const passwordInputElement =
          /** @type {!HTMLElement} */ (passwordInputElements[0]);
      passwordInputElement.value = 'foopassword';
      const confirmPasswordInputElement =
          /** @type {!HTMLElement} */ (passwordInputElements[1]);
      confirmPasswordInputElement.value = passwordInputElement.value;
      triggerInputEvent(passwordInputElement);

      const whenErrorEventFired = eventToPromise('certificates-error', dialog);
      dialog.$$('#ok').click();

      return browserProxy.whenCalled(methodName).then(function() {
        return whenErrorEventFired;
      });
    });
  });

  suite('CertificatePasswordDecryptionDialogTests', function() {
    /** @type {?CertificatePasswordDecryptionDialogElement} */
    let dialog = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    const methodName = 'importPersonalCertificatePasswordSelected';

    setup(function() {
      browserProxy = new TestCertificatesBrowserProxy();
      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      dialog = /** @type {!CertificatePasswordDecryptionDialogElement} */ (
          document.createElement('certificate-password-decryption-dialog'));
      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('DecryptSuccess', function() {
      const passwordInputElement = dialog.$$('#dialog').querySelector('cr-input');
      assertTrue(dialog.$$('#dialog').open);

      // Test that the 'OK' button is enabled even when the password field is
      // empty.
      assertEquals('', passwordInputElement.value);
      assertFalse(dialog.$$('#ok').disabled);

      passwordInputElement.value = 'foopassword';
      assertFalse(dialog.$$('#ok').disabled);

      // Simulate clicking 'OK'.
      dialog.$$('#ok').click();

      return browserProxy.whenCalled(methodName).then(function(password) {
        assertEquals(passwordInputElement.value, password);
        // Check that the dialog is closed.
        assertFalse(dialog.$$('#dialog').open);
      });
    });

    test('DecryptError', function() {
      browserProxy.forceCertificatesError();
      // Simulate entering some password.
      const passwordInputElement = /** @type {!HTMLElement} */ (
          dialog.$$('#dialog').querySelector('cr-input'));
      passwordInputElement.value = 'foopassword';
      triggerInputEvent(passwordInputElement);

      const whenErrorEventFired = eventToPromise('certificates-error', dialog);
      dialog.$$('#ok').click();
      return browserProxy.whenCalled(methodName).then(function() {
        return whenErrorEventFired;
      });
    });
  });

  suite('CertificateSubentryTests', function() {
    /** @type {!CertificateSubentryElement} */
    let subentry;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /**
     * @return {!Promise} A promise firing once |CertificateActionEvent| fires.
     */
    const actionEventToPromise = function() {
      return eventToPromise(CertificateActionEvent, subentry);
    };

    setup(function() {
      browserProxy = new TestCertificatesBrowserProxy();
      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      subentry = /** @type {!CertificateSubentryElement} */ (
          document.createElement('certificate-subentry'));
      subentry.model = createSampleCertificateSubnode();
      subentry.certificateType = CertificateType.PERSONAL;
      document.body.appendChild(subentry);

      // Bring up the popup menu for the following tests to use.
      subentry.$$('#dots').click();
      flush();
    });

    teardown(function() {
      subentry.remove();
    });

    // Test case where 'View' option is tapped.
    test('MenuOptions_View', function() {
      const viewButton = subentry.shadowRoot.querySelector('#view');
      viewButton.click();
      return browserProxy.whenCalled('viewCertificate').then(function(id) {
        assertEquals(subentry.model.id, id);
      });
    });

    // Test that the 'Edit' option is only shown when appropriate and that
    // once tapped the correct event is fired.
    test('MenuOptions_Edit', function() {
      const editButton = subentry.shadowRoot.querySelector('#edit');
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
        const detail =
            /** @type {!CertificateActionEventDetail} */ (event.detail);
        assertEquals(CertificateAction.EDIT, detail.action);
        assertEquals(subentry.model.id, detail.subnode.id);
        assertEquals(subentry.certificateType, detail.certificateType);
      });
    });

    // Test that the 'Delete' option is only shown when appropriate and that
    // once tapped the correct event is fired.
    test('MenuOptions_Delete', function() {
      const deleteButton = subentry.shadowRoot.querySelector('#delete');
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
        const detail =
            /** @type {!CertificateActionEventDetail} */ (event.detail);
        assertEquals(CertificateAction.DELETE, detail.action);
        assertEquals(subentry.model.id, detail.subnode.id);
      });
    });

    // Test that the 'Export' option is always shown when the certificate type
    // is not PERSONAL and that once tapped the correct event is fired.
    test('MenuOptions_Export', function() {
      subentry.certificateType = CertificateType.SERVER;
      const exportButton = subentry.shadowRoot.querySelector('#export');
      assertTrue(!!exportButton);
      assertFalse(exportButton.hidden);
      exportButton.click();
      return browserProxy.whenCalled('exportCertificate').then(function(id) {
        assertEquals(subentry.model.id, id);
      });
    });

    // Test case of exporting a PERSONAL certificate.
    test('MenuOptions_ExportPersonal', function() {
      const exportButton = subentry.shadowRoot.querySelector('#export');
      assertTrue(!!exportButton);

      // Should be disabled when 'model.extractable' is false.
      assertTrue(exportButton.hidden);

      const model = createSampleCertificateSubnode();
      model.extractable = true;
      subentry.model = model;
      assertFalse(exportButton.hidden);

      const waitForActionEvent = actionEventToPromise();
      exportButton.click();
      return browserProxy.whenCalled('exportPersonalCertificate')
          .then(function(id) {
            assertEquals(subentry.model.id, id);

            // A promise firing once |CertificateActionEvent| is
            // fired.
            return waitForActionEvent;
          })
          .then(function(event) {
            const detail =
                /** @type {!CertificateActionEventDetail} */ (event.detail);
            assertEquals(CertificateAction.EXPORT_PERSONAL, detail.action);
            assertEquals(subentry.model.id, detail.subnode.id);
          });
    });
  });

  suite('CertificateManagerTests', function() {
    /** @type {?CertificateManagerElement} */
    let page = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    /** @enum {number} */
    const CertificateCategoryIndex = {
      PERSONAL: 0,
      SERVER: 1,
      CA: 2,
      OTHER: 3,
    };

    setup(function() {
      browserProxy = new TestCertificatesBrowserProxy();
      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      page = /** @type {!CertificateManagerElement} */ (
          document.createElement('certificate-manager'));
      document.body.appendChild(page);
    });

    teardown(function() {
      page.remove();
    });

    /**
     * Test that the page requests information from the browser on startup and
     * that it gets populated accordingly.
     */
    test('Initialization', function() {
      // Trigger all category tabs to be added to the DOM.
      const paperTabsElement = page.shadowRoot.querySelector('cr-tabs');
      paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
      flush();
      paperTabsElement.selected = CertificateCategoryIndex.SERVER;
      flush();
      paperTabsElement.selected = CertificateCategoryIndex.CA;
      flush();
      paperTabsElement.selected = CertificateCategoryIndex.OTHER;
      flush();
      const certificateLists =
          page.shadowRoot.querySelectorAll('certificate-list');
      assertEquals(4, certificateLists.length);

      const assertCertificateListLength = function(listIndex, expectedSize) {
        // Need to switch to the corresponding tab before querying the DOM.
        paperTabsElement.selected = listIndex;
        flush();
        const certificateEntries =
            certificateLists[listIndex].shadowRoot.querySelectorAll(
                'certificate-entry');
        assertEquals(expectedSize, certificateEntries.length);
      };

      assertCertificateListLength(CertificateCategoryIndex.PERSONAL, 0);
      assertCertificateListLength(CertificateCategoryIndex.SERVER, 0);
      assertCertificateListLength(CertificateCategoryIndex.CA, 0);
      assertCertificateListLength(CertificateCategoryIndex.OTHER, 0);

      return browserProxy.whenCalled('refreshCertificates').then(function() {
        // Simulate response for personal and CA certificates.
        webUIListenerCallback(
            'certificates-changed', 'personalCerts',
            [createSampleCertificateOrgGroup()]);
        webUIListenerCallback('certificates-changed', 'caCerts', [
          createSampleCertificateOrgGroup(), createSampleCertificateOrgGroup()
        ]);
        flush();

        assertCertificateListLength(CertificateCategoryIndex.PERSONAL, 1);
        assertCertificateListLength(CertificateCategoryIndex.SERVER, 0);
        assertCertificateListLength(CertificateCategoryIndex.CA, 2);
        assertCertificateListLength(CertificateCategoryIndex.OTHER, 0);
      });
    });

    /**
     * Tests that a dialog opens as a response to a
     * CertificateActionEvent.
     * @param {string} dialogTagName The type of dialog to test.
     * @param {CertificateActionEventDetail} eventDetail
     * @return {!Promise}
     */
    function testDialogOpensOnAction(dialogTagName, eventDetail) {
      assertFalse(!!page.shadowRoot.querySelector(dialogTagName));
      const whenDialogOpen = eventToPromise('cr-dialog-open', page);
      page.fire(CertificateActionEvent, eventDetail);

      // Some dialogs are opened after some async operation to fetch initial
      // data. Ensure that the underlying cr-dialog is actually opened before
      // returning.
      return whenDialogOpen.then(() => {
        assertTrue(!!page.shadowRoot.querySelector(dialogTagName));
      });
    }

    test('OpensDialog_DeleteConfirmation', function() {
      return testDialogOpensOnAction(
          'certificate-delete-confirmation-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.DELETE,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.PERSONAL
          }));
    });

    test('OpensDialog_PasswordEncryption', function() {
      return testDialogOpensOnAction(
          'certificate-password-encryption-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.EXPORT_PERSONAL,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.PERSONAL
          }));
    });

    test('OpensDialog_PasswordDecryption', function() {
      return testDialogOpensOnAction(
          'certificate-password-decryption-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.IMPORT,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.PERSONAL
          }));
    });

    test('OpensDialog_CaTrustEdit', function() {
      return testDialogOpensOnAction(
          'ca-trust-edit-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.EDIT,
            subnode: createSampleCertificateSubnode(),
            certificateType: CertificateType.CA
          }));
    });

    test('OpensDialog_CaTrustImport', function() {
      return testDialogOpensOnAction(
          'ca-trust-edit-dialog',
          /** @type {!CertificateActionEventDetail} */ ({
            action: CertificateAction.IMPORT,
            subnode: {name: 'Dummy Certificate Name', id: null},
            certificateType: CertificateType.CA
          }));
    });

    if (isChromeOS) {
      // Test that import buttons are hidden by default.
      test('ImportButton_Default', function() {
        const paperTabsElement = page.shadowRoot.querySelector('cr-tabs');
        paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
        flush();
        paperTabsElement.selected = CertificateCategoryIndex.CA;
        flush();
        const certificateLists =
            page.shadowRoot.querySelectorAll('certificate-list');
        const clientImportButton = certificateLists[0].$$('#import');
        assertTrue(clientImportButton.hidden);
        const clientImportAndBindButton =
            certificateLists[0].$$('#importAndBind');
        assertTrue(clientImportAndBindButton.hidden);
        const caImportButton = certificateLists[1].$$('#import');
        assertTrue(caImportButton.hidden);
      });

      // Test that ClientCertificateManagementAllowed policy is applied to the
      // UI when management is allowed.
      test('ImportButton_ClientPolicyAllowed', function() {
        const paperTabsElement = page.shadowRoot.querySelector('cr-tabs');
        paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
        flush();
        paperTabsElement.selected = CertificateCategoryIndex.CA;
        flush();
        const certificateLists =
            page.shadowRoot.querySelectorAll('certificate-list');

        return browserProxy.whenCalled('refreshCertificates').then(function() {
          webUIListenerCallback(
              'client-import-allowed-changed', true /* clientImportAllowed */);
          // Verify that import buttons are shown in the client certificate
          // tab.
          const clientImportButton = certificateLists[0].$$('#import');
          assertFalse(clientImportButton.hidden);
          const clientImportAndBindButton =
              certificateLists[0].$$('#importAndBind');
          assertFalse(clientImportAndBindButton.hidden);
          // Verify that import button is still hidden in the CA certificate
          // tab.
          const caImportButton = certificateLists[1].$$('#import');
          assertTrue(caImportButton.hidden);
        });
      });

      // Test that ClientCertificateManagementAllowed policy is applied to the
      // UI when management is not allowed.
      test('ImportButton_ClientPolicyDisallowed', function() {
        const paperTabsElement = page.shadowRoot.querySelector('cr-tabs');
        paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
        flush();
        paperTabsElement.selected = CertificateCategoryIndex.CA;
        flush();
        const certificateLists =
            page.shadowRoot.querySelectorAll('certificate-list');

        return browserProxy.whenCalled('refreshCertificates').then(function() {
          webUIListenerCallback(
              'client-import-allowed-changed', false /* clientImportAllowed */);
          // Verify that import buttons are still hidden in the client
          // certificate tab.
          const clientImportButton = certificateLists[0].$$('#import');
          assertTrue(clientImportButton.hidden);
          const clientImportAndBindButton =
              certificateLists[0].$$('#importAndBind');
          assertTrue(clientImportAndBindButton.hidden);
          // Verify that import button is still hidden in the CA certificate
          // tab.
          const caImportButton = certificateLists[1].$$('#import');
          assertTrue(caImportButton.hidden);
        });
      });

      // Test that CACertificateManagementAllowed policy is applied to the
      // UI when management is allowed.
      test('ImportButton_CAPolicyAllowed', function() {
        const paperTabsElement = page.shadowRoot.querySelector('cr-tabs');
        paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
        flush();
        paperTabsElement.selected = CertificateCategoryIndex.CA;
        flush();
        const certificateLists =
            page.shadowRoot.querySelectorAll('certificate-list');

        return browserProxy.whenCalled('refreshCertificates').then(function() {
          webUIListenerCallback(
              'ca-import-allowed-changed', true /* clientImportAllowed */);
          // Verify that import buttons are still hidden in the client
          // certificate tab.
          const clientImportButton = certificateLists[0].$$('#import');
          assertTrue(clientImportButton.hidden);
          const clientImportAndBindButton =
              certificateLists[0].$$('#importAndBind');
          assertTrue(clientImportAndBindButton.hidden);
          // Verify that import button is shown in the CA certificate tab.
          const caImportButton = certificateLists[1].$$('#import');
          assertFalse(caImportButton.hidden);
        });
      });

      // Test that CACertificateManagementAllowed policy is applied to the
      // UI when management is not allowed.
      test('ImportButton_CAPolicyDisallowed', function() {
        const paperTabsElement = page.shadowRoot.querySelector('cr-tabs');
        paperTabsElement.selected = CertificateCategoryIndex.PERSONAL;
        flush();
        paperTabsElement.selected = CertificateCategoryIndex.CA;
        flush();
        const certificateLists =
            page.shadowRoot.querySelectorAll('certificate-list');

        return browserProxy.whenCalled('refreshCertificates').then(function() {
          webUIListenerCallback(
              'ca-import-allowed-changed', false /* clientImportAllowed */);
          // Verify that import buttons are still hidden in the client
          // certificate tab.
          const clientImportButton = certificateLists[0].$$('#import');
          assertTrue(clientImportButton.hidden);
          const clientImportAndBindButton =
              certificateLists[0].$$('#importAndBind');
          assertTrue(clientImportAndBindButton.hidden);
          // Verify that import button is still hidden in the CA certificate
          // tab.
          const caImportButton = certificateLists[1].$$('#import');
          assertTrue(caImportButton.hidden);
        });
      });
    }
  });

  suite('CertificateListTests', function() {
    /** @type {?CertificateListElement} */
    let element = null;

    /** @type {?TestCertificatesBrowserProxy} */
    let browserProxy = null;

    setup(function() {
      browserProxy = new TestCertificatesBrowserProxy();
      CertificatesBrowserProxyImpl.instance_ = browserProxy;
      document.body.innerHTML = '';
      element = /** @type {!CertificateListElement} */ (
          document.createElement('certificate-list'));
      document.body.appendChild(element);
    });

    teardown(function() {
      element.remove();
    });

    /**
     * Tests the "Import" button functionality.
     * @param {!CertificateType} certificateType
     * @param {string} proxyMethodName The name of the proxy method expected
     *     to be called.
     * @param {boolean} actionEventExpected Whether a
     *     CertificateActionEvent is expected to fire as a result tapping the
     *     Import button.
     * @param {boolean} bindBtn Whether to click on the import and bind btn.
     */
    function testImportForCertificateType(
        certificateType, proxyMethodName, actionEventExpected, bindBtn) {
      element.certificateType = certificateType;
      flush();

      const importButton =
          bindBtn ? element.$$('#importAndBind') : element.$$('#import');
      assertTrue(!!importButton);

      const waitForActionEvent = actionEventExpected ?
          eventToPromise(CertificateActionEvent, element) :
          Promise.resolve(null);

      importButton.click();
      return browserProxy.whenCalled(proxyMethodName)
          .then(function(arg) {
            if (proxyMethodName === 'importPersonalCertificate') {
              assertNotEquals(arg, undefined);
              assertEquals(arg, bindBtn);
            }
            return waitForActionEvent;
          })
          .then(function(event) {
            if (actionEventExpected) {
              assertEquals(CertificateAction.IMPORT, event.detail.action);
              assertEquals(certificateType, event.detail.certificateType);
            }
          });
    }

    test('ImportButton_Personal', function() {
      return testImportForCertificateType(
          CertificateType.PERSONAL, 'importPersonalCertificate', true, false);
    });

    if (isChromeOS) {
      test('ImportAndBindButton_Personal', function() {
        return testImportForCertificateType(
            CertificateType.PERSONAL, 'importPersonalCertificate', true, true);
      });
    }

    test('ImportButton_Server', function() {
      return testImportForCertificateType(
          CertificateType.SERVER, 'importServerCertificate', false, false);
    });

    test('ImportButton_CA', function() {
      return testImportForCertificateType(
          CertificateType.CA, 'importCaCertificate', true, false);
    });
  });
