// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests of CrOS specific saved password settings. Note that
 * although these tests for only for CrOS, they are testing a CrOS specific
 * aspects of the implementation of a browser feature rather than an entirely
 * native CrOS feature. See http://crbug.com/917178 for more detail.
 */

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BlockingRequestManager} from 'chrome://settings/lazy_load.js';
import {MultiStorePasswordUiEntry, PasswordManagerImpl} from 'chrome://settings/settings.js';
import {MockTimer} from 'chrome://test/mock_timer.js';
import {createPasswordEntry, PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {runCancelExportTest, runExportFlowErrorRetryTest, runExportFlowErrorTest, runExportFlowFastTest, runExportFlowSlowTest, runFireCloseEventAfterExportCompleteTest,runStartExportTest} from './passwords_export_test.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

suite('PasswordsSection_Cros', function() {
  /**
   * Promise resolved when an auth token request is made.
   * @type {Promise}
   */
  let requestPromise = null;

  /**
   * Promise resolved when a saved password is retrieved.
   * @type {Promise}
   */
  let passwordPromise = null;

  /**
   * Implementation of PasswordSectionElementFactory with parameters that help
   * tests to track auth token and saved password requests.
   */
  class CrosPasswordSectionElementFactory extends
      PasswordSectionElementFactory {
    /**
     * @param {HTMLDocument} document The test's |document| object.
     * @param {request: Function} tokenRequestManager Fake for
     *     BlockingRequestManager provided to subelements of password-section
     *     that normally have their tokenRequestManager property bound to the
     *     password section's tokenRequestManager_ property. Note:
     *     Tests of the password-section element need to use the full
     *     implementation, which is created by default when the element is
     *     attached.
     * @param {MultiStorePasswordUiEntry} passwordItem
     */
    constructor(document, tokenRequestManager, passwordItem) {
      super(document);
      this.tokenRequestManager = tokenRequestManager;
      this.passwordItem = passwordItem;
    }

    /** @override */
    createPasswordsSection(passwordManager) {
      return super.createPasswordsSection(passwordManager, [], []);
    }

    /** @override */
    createPasswordEditDialog() {
      return this.decorateShowPasswordElement_('password-edit-dialog');
    }

    /** @override */
    createPasswordListItem() {
      return this.decorateShowPasswordElement_('password-list-item');
    }

    /** @override */
    createExportPasswordsDialog(passwordManager, overrideRequestManager) {
      const dialog = super.createExportPasswordsDialog(passwordManager);
      dialog.tokenRequestManager = new BlockingRequestManager();
      return overrideRequestManager ?
          Object.assign(
              dialog, {tokenRequestManager: this.tokenRequestManager}) :
          dialog;
    }

    /**
     * Creates an element with ShowPasswordBehavior, decorates it with
     * with the testing data provided in the constructor, and attaches it to
     * |this.document|.
     * @param {string} showPasswordElementName Tag name of a Polymer element
     *     with ShowPasswordBehavior.
     * @return {!HTMLElement} Element decorated with test data.
     */
    decorateShowPasswordElement_(showPasswordElementName) {
      const element = this.document.createElement(showPasswordElementName);
      element.item = this.passwordItem;
      element.tokenRequestManager = this.tokenRequestManager;
      this.document.body.appendChild(element);
      flush();
      return element;
    }
  }

  function fail() {
    throw new Error();
  }

  /** @type {TestPasswordManagerProxy} */
  let passwordManager = null;

  /** @type {CrosPasswordSectionElementFactory} */
  let elementFactory = null;

  setup(function() {
    PolymerTest.clearBody();
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    // Define a fake BlockingRequestManager to track when a token request
    // comes in by resolving requestPromise.
    let requestManager;
    requestPromise = new Promise(resolve => {
      requestManager = {request: resolve};
    });

    /**
     * @type {ShowPasswordBehavior.UiEntryWithPassword} Password item (i.e.
     *      entry with password text) that overrides the password property
     *      with get/set to track receipt of a saved password and make that
     *      information available by resolving |passwordPromise|.
     */
    let passwordItem;
    passwordPromise = new Promise(resolve => {
      passwordItem = {
        entry: createPasswordEntry(),
        set password(newPassword) {
          if (newPassword && newPassword !== this.password_) {
            resolve(newPassword);
          }
          this.password_ = newPassword;
        },
        get password() {
          return this.password_;
        }
      };
    });

    elementFactory = new CrosPasswordSectionElementFactory(
        document, requestManager, passwordItem);
  });

  // Note (rbpotter): this passes locally, but may still be flaky (see
  // https://www.crbug.com/1021474)
  test.skip('export passwords button requests auth token', function() {
    passwordPromise.then(fail);
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, true);
    exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
    return requestPromise;
  });

  // Note (rbpotter): this passes locally, but may still be flaky (see
  // https://www.crbug.com/1021474)
  test.skip(
      'list-item does not request token if it gets password to show',
      function() {
        requestPromise.then(fail);
        const passwordListItem = elementFactory.createPasswordListItem();
        passwordManager.setPlaintextPassword('password');
        passwordListItem.shadowRoot.querySelector('#showPasswordButton')
            .click();
        return passwordPromise;
      });

  // Note (rbpotter): this fails locally, possibly out of date
  test.skip(
      'list-item requests token if it does not get password to show',
      function() {
        passwordPromise.then(fail);
        const passwordListItem = elementFactory.createPasswordListItem();
        passwordManager.setPlaintextPassword('');
        passwordListItem.shadowRoot.querySelector('#showPasswordButton')
            .click();
        return requestPromise;
      });

  // TODO(crbug.com/1274569): add test for edit-dialog requesting token when
  // switching from ADD to EDIT mode when other tests are fixed.

  // Note (rbpotter): this passes locally, but may still be flaky (see
  // https://www.crbug.com/1021474)
  test.skip(
      'edit-dialog does not request token if it gets password to show',
      function() {
        requestPromise.then(fail);
        const passwordEditDialog = elementFactory.createPasswordEditDialog();
        passwordManager.setPlaintextPassword('password');
        passwordEditDialog.shadowRoot.querySelector('#showPasswordButton')
            .click();
        return passwordPromise;
      });

  // Note (rbpotter): this fails locally, possibly out of date
  test.skip(
      'edit-dialog requests token if it does not get password to show',
      function() {
        passwordPromise.then(fail);
        const passwordEditDialog = elementFactory.createPasswordEditDialog();
        passwordManager.setPlaintextPassword('');
        passwordEditDialog.shadowRoot.querySelector('#showPasswordButton')
            .click();
        return requestPromise;
      });

  // Note (rbpotter): this passes locally, but may still be flaky (see
  // https://www.crbug.com/1021474)
  test.skip('password-prompt-dialog appears on auth token request', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager);
    assertTrue(!passwordsSection.shadowRoot.querySelector(
        'settings-password-prompt-dialog'));
    passwordsSection.tokenRequestManager_.request(fail);
    flush();
    assertTrue(!!passwordsSection.shadowRoot.querySelector(
        'settings-password-prompt-dialog'));
  });

  // Note (rbpotter): this fails locally, possibly out of date
  test.skip(
      'password-section resolves request on auth token receipt',
      function(done) {
        const passwordsSection =
            elementFactory.createPasswordsSection(passwordManager);
        passwordsSection.tokenRequestManager_.request(done);
        passwordsSection.authToken_ = 'auth token';
      });

  // Note (rbpotter): this fails locally, possibly out of date
  test.skip(
      'password-section only triggers callback on most recent request',
      function(done) {
        const passwordsSection =
            elementFactory.createPasswordsSection(passwordManager);
        // Make request that SHOULD NOT be resolved.
        passwordsSection.tokenRequestManager_.request(fail);
        // Make request that should be resolved.
        passwordsSection.tokenRequestManager_.request(done);
        passwordsSection.authToken_ = 'auth token';
      });

  // Note (rbpotter): this fails locally, possibly out of date
  test.skip(
      'user is not prompted for password if they cannot enter it',
      function(done) {
        loadTimeData.overrideValues({userCannotManuallyEnterPassword: true});
        const passwordsSection = document.createElement('passwords-section');
        document.body.appendChild(passwordsSection);
        flush();
        assertTrue(!passwordsSection.shadowRoot.querySelector(
            'settings-password-prompt-dialog'));
        passwordsSection.tokenRequestManager_.request(() => {
          flush();
          assertTrue(!passwordsSection.shadowRoot.querySelector(
              'settings-password-prompt-dialog'));
          done();
        });
      });

  // Test that tapping "Export passwords..." notifies the browser.
  test('startExport', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    runStartExportTest(exportDialog, passwordManager, done);
  });

  // Test the export flow. If exporting is fast, we should skip the
  // in-progress view altogether.
  test('exportFlowFast', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    runExportFlowFastTest(exportDialog, passwordManager, done);
  });

  // The error view is shown when an error occurs.
  test('exportFlowError', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    runExportFlowErrorTest(exportDialog, passwordManager, done);
  });

  // The error view allows to retry.
  test('exportFlowErrorRetry', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    runExportFlowErrorRetryTest(exportDialog, passwordManager, done);
  });

  // Test the export flow. If exporting is slow, Chrome should show the
  // in-progress dialog for at least 1000ms.
  test('exportFlowSlow', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    runExportFlowSlowTest(exportDialog, passwordManager, done);
  });

  // Test that canceling the dialog while exporting will also cancel the
  // export on the browser.
  test('cancelExport', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    runCancelExportTest(exportDialog, passwordManager, done);
  });

  test('fires close event after export complete', () => {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager, false);
    return runFireCloseEventAfterExportCompleteTest(
        exportDialog, passwordManager);
  });
});
