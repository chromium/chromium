// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests of CrOS specific saved password settings. Note that
 * although these tests for only for CrOS, they are testing a CrOS specific
 * aspects of the implementation of a browser feature rather than an entirely
 * native CrOS feature. See http://crbug.com/917178 for more detail.
 */

// clang-format off
import {BlockingRequestManager} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl} from 'chrome://settings/settings.js';

import {createPasswordEntry, PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {runCancelExportTest, runExportFlowErrorRetryTest, runExportFlowErrorTest, runExportFlowFastTest, runExportFlowSlowTest, runFireCloseEventAfterExportCompleteTest,runStartExportTest} from './passwords_export_dialog_test.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

suite('PasswordsSection_Cros', function() {
  /**
   * Promise resolved when an auth token request is made.
   */
  let requestPromise: Promise<void>;

  /**
   * Promise resolved when a saved password is retrieved.
   */
  let passwordPromise: Promise<void>;

  class PasswordItem {
    private password_: string = '';
    entry: chrome.passwordsPrivate.PasswordUiEntry = createPasswordEntry();
    resolve: (p: void) => void;

    constructor(callback: (p: void) => void) {
      this.resolve = callback;
    }

    set password(newPassword: string) {
      if (newPassword && newPassword !== this.password_) {
        this.resolve();
      }
      this.password_ = newPassword;
    }

    get password(): string {
      return this.password_;
    }
  }

  /**
   * Implementation of PasswordSectionElementFactory with parameters that help
   * tests to track auth token and saved password requests.
   */
  class CrosPasswordSectionElementFactory extends
      PasswordSectionElementFactory {
    tokenRequestManager: {request: (p: any) => void}|null;
    passwordItem: PasswordItem|null;

    /**
     * @param document The test's |document| object.
     * @param tokenRequestManager Fake for
     *     BlockingRequestManager provided to subelements of password-section
     *     that normally have their tokenRequestManager property bound to the
     *     password section's tokenRequestManager_ property. Note:
     *     Tests of the password-section element need to use the full
     *     implementation, which is created by default when the element is
     *     attached.
     */
    constructor(
        document: HTMLDocument,
        tokenRequestManager: {request: (p: any) => void}|null,
        passwordItem: PasswordItem|null) {
      super(document);
      this.tokenRequestManager = tokenRequestManager;
      this.passwordItem = passwordItem;
    }

    override createPasswordsSection(
        passwordManager: TestPasswordManagerProxy,
        _passwordList: chrome.passwordsPrivate.PasswordUiEntry[],
        _exceptionList: chrome.passwordsPrivate.ExceptionEntry[]) {
      return super.createPasswordsSection(passwordManager, [], []);
    }

    override createExportPasswordsDialog(overrideRequestManager?: boolean) {
      const dialog = super.createExportPasswordsDialog();
      dialog.tokenRequestManager = new BlockingRequestManager();
      return overrideRequestManager ?
          Object.assign(
              dialog, {tokenRequestManager: this.tokenRequestManager}) :
          dialog;
    }
  }

  function fail() {
    throw new Error();
  }

  let passwordManager: TestPasswordManagerProxy;

  let elementFactory: CrosPasswordSectionElementFactory;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    // Define a fake BlockingRequestManager to track when a token request
    // comes in by resolving requestPromise.
    let requestManager: {request: (p: any) => void}|null = null;
    requestPromise = new Promise(resolve => {
      requestManager = {request: resolve};
    });

    /**
     *      Password item (i.e.
     *      entry with password text) that overrides the password property
     *      with get/set to track receipt of a saved password and make that
     *      information available by resolving |passwordPromise|.
     */
    let passwordItem: PasswordItem|null = null;
    passwordPromise = new Promise(resolve => {
      passwordItem = new PasswordItem(resolve);
    });

    elementFactory = new CrosPasswordSectionElementFactory(
        document, requestManager, passwordItem);
  });

  // Note (rbpotter): this passes locally, but may still be flaky (see
  // https://www.crbug.com/1021474)
  test.skip('export passwords button requests auth token', function() {
    passwordPromise.then(fail);
    const exportDialog = elementFactory.createExportPasswordsDialog(true);
    exportDialog.shadowRoot!
        .querySelector<HTMLElement>('#exportPasswordsButton')!.click();
    return requestPromise;
  });

  // Test that tapping "Export passwords" notifies the browser.
  test('startExport', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    runStartExportTest(exportDialog, passwordManager);
  });

  // Test the export flow. If exporting is fast, we should skip the
  // in-progress view altogether.
  test('exportFlowFast', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    runExportFlowFastTest(exportDialog, passwordManager);
  });

  // The error view is shown when an error occurs.
  test('exportFlowError', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    runExportFlowErrorTest(exportDialog, passwordManager);
  });

  // The error view allows to retry.
  test('exportFlowErrorRetry', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    runExportFlowErrorRetryTest(exportDialog, passwordManager);
  });

  // Test the export flow. If exporting is slow, Chrome should show the
  // in-progress dialog for at least 1000ms.
  test('exportFlowSlow', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    runExportFlowSlowTest(exportDialog, passwordManager);
  });

  // Test that canceling the dialog while exporting will also cancel the
  // export on the browser.
  test('cancelExport', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    runCancelExportTest(exportDialog, passwordManager);
  });

  test('fires close event after export complete', () => {
    const exportDialog = elementFactory.createExportPasswordsDialog(false);
    return runFireCloseEventAfterExportCompleteTest(
        exportDialog, passwordManager);
  });
});
