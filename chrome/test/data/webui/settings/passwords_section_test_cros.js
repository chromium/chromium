// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests of CrOS specific saved password settings. Note that
 * although these tests for only for CrOS, they are testing a CrOS specific
 * aspects of the implementation of a browser feature rather than an entirely
 * native CrOS feature. See http://crbug.com/917178 for more detail.
 */

cr.define('settings_passwords_section_cros', function() {
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
       * @param {ShowPasswordBehavior.UiEntryWithPassword} passwordItem Wrapper
       *     for a PasswordUiEntry and the corresponding password.
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
      createExportPasswordsDialog(passwordManager) {
        return Object.assign(
            super.createExportPasswordsDialog(passwordManager),
            {tokenRequestManager: this.tokenRequestManager});
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
        Polymer.dom.flush();
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
      PasswordManagerImpl.instance_ = passwordManager;

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
          entry: FakeDataMaker.passwordEntry(),
          set password(newPassword) {
            if (newPassword && newPassword != this.password_) {
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

    test('export passwords button requests auth token', function() {
      passwordPromise.then(fail);
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      exportDialog.$$('#exportPasswordsButton').click();
      return requestPromise;
    });

    test(
        'list-item does not request token if it gets password to show',
        function() {
          requestPromise.then(fail);
          const passwordListItem = elementFactory.createPasswordListItem();
          passwordManager.setPlaintextPassword('password');
          passwordListItem.$$('#showPasswordButton').click();
          return passwordPromise;
        });

    test(
        'list-item requests token if it does not get password to show',
        function() {
          passwordPromise.then(fail);
          const passwordListItem = elementFactory.createPasswordListItem();
          passwordManager.setPlaintextPassword('');
          passwordListItem.$$('#showPasswordButton').click();
          return requestPromise;
        });

    test(
        'edit-dialog does not request token if it gets password to show',
        function() {
          requestPromise.then(fail);
          const passwordEditDialog = elementFactory.createPasswordEditDialog();
          passwordManager.setPlaintextPassword('password');
          passwordEditDialog.$$('#showPasswordButton').click();
          return passwordPromise;
        });

    test(
        'edit-dialog requests token if it does not get password to show',
        function() {
          passwordPromise.then(fail);
          const passwordEditDialog = elementFactory.createPasswordEditDialog();
          passwordManager.setPlaintextPassword('');
          passwordEditDialog.$$('#showPasswordButton').click();
          return requestPromise;
        });

    test('password-prompt-dialog appears on auth token request', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager);
      assertTrue(!passwordsSection.$$('settings-password-prompt-dialog'));
      passwordsSection.tokenRequestManager_.request(fail);
      Polymer.dom.flush();
      assertTrue(!!passwordsSection.$$('settings-password-prompt-dialog'));
    });

    test(
        'password-section resolves request on auth token receipt',
        function(done) {
          const passwordsSection =
              elementFactory.createPasswordsSection(passwordManager);
          passwordsSection.tokenRequestManager_.request(done);
          passwordsSection.authToken_ = 'auth token';
        });

    test(
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

    test(
        'user is not prompted for password if they cannot enter it',
        function(done) {
          loadTimeData.overrideValues({userCannotManuallyEnterPassword: true});
          const passwordsSection = document.createElement('passwords-section');
          document.body.appendChild(passwordsSection);
          Polymer.dom.flush();
          assertTrue(!passwordsSection.$$('settings-password-prompt-dialog'));
          passwordsSection.tokenRequestManager_.request(() => {
            Polymer.dom.flush();
            assertTrue(!passwordsSection.$$('settings-password-prompt-dialog'));
            done();
          });
        });
  });
});
