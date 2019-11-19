// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */


cr.define('settings_passwords_section', function() {
  /**
   * Helper method that validates a that elements in the password list match
   * the expected data.
   * @param {!Element} listElement The iron-list element that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList The
   *     expected data.
   * @private
   */
  function validatePasswordList(listElement, passwordList) {
    assertEquals(passwordList.length, listElement.items.length);
    for (let index = 0; index < passwordList.length; ++index) {
      // The first child is a template, skip and get the real 'first child'.
      const node = Polymer.dom(listElement).children[index + 1];
      assert(node);
      const passwordInfo = passwordList[index];
      assertEquals(
          passwordInfo.urls.shown, node.$$('#originUrl').textContent.trim());
      assertEquals(passwordInfo.urls.link, node.$$('#originUrl').href);
      assertEquals(passwordInfo.username, node.$$('#username').value);
      assertEquals(
          passwordInfo.numCharactersInPassword,
          node.$$('#password').value.length);
      assertDeepEquals(listElement.items[index].entry, passwordInfo);
    }
  }

  /**
   * Helper method that validates a that elements in the exception list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList The
   *     expected data.
   * @private
   */
  function validateExceptionList(nodes, exceptionList) {
    assertEquals(exceptionList.length, nodes.length);
    for (let index = 0; index < exceptionList.length; ++index) {
      const node = nodes[index];
      const exception = exceptionList[index];
      assertEquals(
          exception.urls.shown,
          node.querySelector('#exception').textContent.trim());
      assertEquals(
          exception.urls.link.toLowerCase(),
          node.querySelector('#exception').href);
    }
  }

  /**
   * Returns all children of an element that has children added by a dom-repeat.
   * @param {!Element} element
   * @return {!Array<!Element>}
   * @private
   */
  function getDomRepeatChildren(element) {
    const nodes = element.querySelectorAll('.list-item:not([id])');
    return nodes;
  }

  /**
   * Extracts the first password-list-item in the a password-section element.
   * @param {!Element} passwordsSection
   */
  function getFirstPasswordListItem(passwordsSection) {
    // The first child is a template, skip and get the real 'first child'.
    return Polymer.dom(passwordsSection.$.passwordList).children[1];
  }

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {string} url The URL that is being searched for.
   */
  function listContainsUrl(passwordList, url) {
    for (let i = 0; i < passwordList.length; ++i) {
      if (passwordList[i].urls.origin == url) {
        return true;
      }
    }
    return false;
  }

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @param {string} url The URL that is being searched for.
   */
  function exceptionsListContainsUrl(exceptionList, url) {
    for (let i = 0; i < exceptionList.length; ++i) {
      if (exceptionList[i].urls.orginUrl == url) {
        return true;
      }
    }
    return false;
  }

  suite('PasswordsSection', function() {
    /** @type {TestPasswordManagerProxy} */
    let passwordManager = null;

    /** @type {PasswordSectionElementFactory} */
    let elementFactory = null;

    setup(function() {
      PolymerTest.clearBody();
      // Override the PasswordManagerImpl for testing.
      passwordManager = new TestPasswordManagerProxy();
      PasswordManagerImpl.instance_ = passwordManager;
      elementFactory = new PasswordSectionElementFactory(document);
    });

    test('testPasswordsExtensionIndicator', function() {
      // Initialize with dummy prefs.
      const element = document.createElement('passwords-section');
      element.prefs = {
        credentials_enable_service: {},
        profile: {password_manager_leak_detection: {}},
      };
      document.body.appendChild(element);

      assertFalse(!!element.$$('#passwordsExtensionIndicator'));
      element.set('prefs.credentials_enable_service.extensionId', 'test-id');
      Polymer.dom.flush();

      assertTrue(!!element.$$('#passwordsExtensionIndicator'));
    });

    test('verifyNoSavedPasswords', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      validatePasswordList(passwordsSection.$.passwordList, []);

      assertFalse(passwordsSection.$.noPasswordsLabel.hidden);
      assertTrue(passwordsSection.$.savedPasswordsHeaders.hidden);
    });

    test('verifySavedPasswordLength', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('site1.com', 'luigi', 1),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7),
        FakeDataMaker.passwordEntry('site2.com', 'mario', 70),
        FakeDataMaker.passwordEntry('site1.com', 'peach', 11),
        FakeDataMaker.passwordEntry('google.com', 'mario', 7),
        FakeDataMaker.passwordEntry('site2.com', 'luigi', 8),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      // Assert that the data is passed into the iron list. If this fails,
      // then other expectations will also fail.
      assertDeepEquals(
          passwordList,
          passwordsSection.$.passwordList.items.map(entry => entry.entry));

      validatePasswordList(passwordsSection.$.passwordList, passwordList);

      assertTrue(passwordsSection.$.noPasswordsLabel.hidden);
      assertFalse(passwordsSection.$.savedPasswordsHeaders.hidden);
    });

    // Test verifies that removing a password will update the elements.
    test('verifyPasswordListRemove', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('anotherwebsite.com', 'luigi', 1, 0),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7, 1),
        FakeDataMaker.passwordEntry('website.com', 'mario', 70, 2)
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
      // Simulate 'longwebsite.com' being removed from the list.
      passwordList.splice(1, 1);
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();

      assertFalse(listContainsUrl(
          passwordsSection.savedPasswords.map(entry => entry.entry),
          'longwebsite.com'));
      assertFalse(listContainsUrl(passwordList, 'longwebsite.com'));

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
    });

    // Test verifies that adding a password will update the elements.
    test('verifyPasswordListAdd', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('anotherwebsite.com', 'luigi', 1, 0),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7, 1),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
      // Simulate 'website.com' being added to the list.
      passwordList.unshift(
          FakeDataMaker.passwordEntry('website.com', 'mario', 70, 2));
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
    });

    // Test verifies that removing one out of two passwords for the same website
    // will update the elements.
    test('verifyPasswordListRemoveSameWebsite', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      // Set-up initial list.
      let passwordList = [
        FakeDataMaker.passwordEntry('website.com', 'mario', 1, 0),
        FakeDataMaker.passwordEntry('website.com', 'luigi', 7, 1)
      ];

      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection.$.passwordList, passwordList);

      // Simulate '(website.com, mario)' being removed from the list.
      passwordList.shift();
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection.$.passwordList, passwordList);

      // Simulate '(website.com, luigi)' being removed from the list as well.
      passwordList = [];
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection.$.passwordList, passwordList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any passwords.
    test('verifyPasswordItemRemoveButton', function(done) {
      const passwordList = [
        FakeDataMaker.passwordEntry('one', 'six', 5),
        FakeDataMaker.passwordEntry('two', 'five', 3),
        FakeDataMaker.passwordEntry('three', 'four', 1),
        FakeDataMaker.passwordEntry('four', 'three', 2),
        FakeDataMaker.passwordEntry('five', 'two', 4),
        FakeDataMaker.passwordEntry('six', 'one', 6),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      const firstNode = getFirstPasswordListItem(passwordsSection);
      assert(firstNode);
      const firstPassword = passwordList[0];

      passwordManager.onRemoveSavedPassword = function(id) {
        // Verify that the event matches the expected value.
        assertEquals(firstPassword.id, id);

        // Clean up after self.
        passwordManager.onRemoveSavedPassword = null;

        done();
      };

      // Click the remove button on the first password.
      firstNode.$$('#passwordMenu').click();
      passwordsSection.$.menuRemovePassword.click();
    });

    test('verifyFilterPasswords', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1),
        FakeDataMaker.passwordEntry('four.com', 'three', 2),
        FakeDataMaker.passwordEntry('five.com', 'two', 4),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      passwordsSection.filter = 'SHow';
      Polymer.dom.flush();

      const expectedList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6),
      ];

      validatePasswordList(passwordsSection.$.passwordList, expectedList);
    });

    test('verifyFilterPasswordsWithRemoval', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5, 0),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3, 1),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1, 2),
        FakeDataMaker.passwordEntry('four.com', 'three', 2, 3),
        FakeDataMaker.passwordEntry('five.com', 'two', 4, 4),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6, 5),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      passwordsSection.filter = 'SHow';
      Polymer.dom.flush();

      let expectedList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5, 0),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3, 1),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1, 2),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6, 5),
      ];

      validatePasswordList(passwordsSection.$.passwordList, expectedList);

      // Simulate removal of three.com/show
      passwordList.splice(2, 1);

      expectedList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5, 0),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3, 1),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6, 5),
      ];

      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection.$.passwordList, expectedList);
    });

    test('verifyFilterPasswordExceptions', function() {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docsshoW.google.com'),
        FakeDataMaker.exceptionEntry('showmail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('mapsshow.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.comshow'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [], exceptionList);
      passwordsSection.filter = 'shOW';
      Polymer.dom.flush();

      const expectedExceptionList = [
        FakeDataMaker.exceptionEntry('docsshoW.google.com'),
        FakeDataMaker.exceptionEntry('showmail.com'),
        FakeDataMaker.exceptionEntry('mapsshow.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.comshow'),
      ];

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          expectedExceptionList);
    });

    test('verifyNoPasswordExceptions', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList), []);

      assertFalse(passwordsSection.$.noExceptionsLabel.hidden);
    });

    test('verifyPasswordExceptions', function() {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [], exceptionList);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      assertTrue(passwordsSection.$.noExceptionsLabel.hidden);
    });

    // Test verifies that removing an exception will update the elements.
    test('verifyPasswordExceptionRemove', function() {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [], exceptionList);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      // Simulate 'mail.com' being removed from the list.
      passwordsSection.splice('passwordExceptions', 1, 1);
      assertFalse(exceptionsListContainsUrl(
          passwordsSection.passwordExceptions, 'mail.com'));
      assertFalse(exceptionsListContainsUrl(exceptionList, 'mail.com'));
      Polymer.dom.flush();

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any exceptions.
    test('verifyPasswordExceptionRemoveButton', function(done) {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [], exceptionList);

      const exceptions =
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList);

      // The index of the button currently being checked.
      let item = 0;

      const clickRemoveButton = function() {
        exceptions[item].querySelector('#removeExceptionButton').click();
      };

      passwordManager.onRemoveException = function(id) {
        // Verify that the event matches the expected value.
        assertTrue(item < exceptionList.length);
        assertEquals(id, exceptionList[item].id);

        if (++item < exceptionList.length) {
          clickRemoveButton();  // Click 'remove' on all passwords, one by one.
        } else {
          // Clean up after self.
          passwordManager.onRemoveException = null;

          done();
        }
      };

      // Start removing.
      clickRemoveButton();
    });

    test('verifyFederatedPassword', function() {
      const item = FakeDataMaker.passwordEntry('goo.gl', 'bart', 0);
      item.federationText = 'with chromium.org';
      const passwordDialog = elementFactory.createPasswordEditDialog(item);

      Polymer.dom.flush();

      assertEquals(item.federationText, passwordDialog.$.passwordInput.value);
      // Text should be readable.
      assertEquals('text', passwordDialog.$.passwordInput.type);
      assertTrue(passwordDialog.$.showPasswordButton.hidden);
    });

    test('showSavedPasswordEditDialog', function() {
      const PASSWORD = 'bAn@n@5';
      const item =
          FakeDataMaker.passwordEntry('goo.gl', 'bart', PASSWORD.length);
      const passwordDialog = elementFactory.createPasswordEditDialog(item);

      assertFalse(passwordDialog.$.showPasswordButton.hidden);

      passwordDialog.set('item.password', PASSWORD);
      Polymer.dom.flush();

      assertEquals(PASSWORD, passwordDialog.$.passwordInput.value);
      // Password should be visible.
      assertEquals('text', passwordDialog.$.passwordInput.type);
      assertFalse(passwordDialog.$.showPasswordButton.hidden);
    });

    test('showSavedPasswordListItem', function() {
      const PASSWORD = 'bAn@n@5';
      const item =
          FakeDataMaker.passwordEntry('goo.gl', 'bart', PASSWORD.length);
      const passwordListItem = elementFactory.createPasswordListItem(item);
      // Hidden passwords should be disabled.
      assertTrue(passwordListItem.$$('#password').disabled);

      passwordListItem.set('item.password', PASSWORD);
      Polymer.dom.flush();

      assertEquals(PASSWORD, passwordListItem.$$('#password').value);
      // Password should be visible.
      assertEquals('text', passwordListItem.$$('#password').type);
      // Visible passwords should not be disabled.
      assertFalse(passwordListItem.$$('#password').disabled);

      // Hide Password Button should be shown.
      assertTrue(passwordListItem.$$('#showPasswordButton')
                     .classList.contains('icon-visibility-off'));
    });

    // Tests that invoking the plaintext password sets the corresponding
    // password.
    test('onShowSavedPasswordEditDialog', function() {
      const expectedItem = FakeDataMaker.passwordEntry('goo.gl', 'bart', 8, 1);
      const passwordDialog =
          elementFactory.createPasswordEditDialog(expectedItem);
      assertEquals('', passwordDialog.item.password);

      passwordManager.setPlaintextPassword('password');
      passwordDialog.$.showPasswordButton.click();
      return passwordManager.whenCalled('getPlaintextPassword').then(id => {
        assertEquals(1, id);
        assertEquals('password', passwordDialog.item.password);
      });
    });

    test('onShowSavedPasswordListItem', function() {
      const expectedItem = FakeDataMaker.passwordEntry('goo.gl', 'bart', 8, 1);
      const passwordListItem =
          elementFactory.createPasswordListItem(expectedItem);
      assertEquals('', passwordListItem.item.password);

      passwordManager.setPlaintextPassword('password');
      passwordListItem.$$('#showPasswordButton').click();
      return passwordManager.whenCalled('getPlaintextPassword').then(id => {
        assertEquals(1, id);
        assertEquals('password', passwordListItem.item.password);
      });
    });

    test('closingPasswordsSectionHidesUndoToast', function(done) {
      const passwordEntry = FakeDataMaker.passwordEntry('goo.gl', 'bart', 1);
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [passwordEntry], []);
      const toastManager = cr.toastManager.getInstance();

      // Click the remove button on the first password and assert that an undo
      // toast is shown.
      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      passwordsSection.$.menuRemovePassword.click();
      assertTrue(toastManager.isToastOpen);

      // Remove the passwords section from the DOM and check that this closes
      // the undo toast.
      document.body.removeChild(passwordsSection);
      assertFalse(toastManager.isToastOpen);

      done();
    });

    // Chrome offers the export option when there are passwords.
    test('offerExportWhenPasswords', function(done) {
      const passwordList = [
        FakeDataMaker.passwordEntry('googoo.com', 'Larry', 1),
      ];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
      assertFalse(passwordsSection.$.menuExportPassword.hidden);
      done();
    });

    // Chrome shouldn't offer the option to export passwords if there are no
    // passwords.
    test('noExportIfNoPasswords', function(done) {
      const passwordList = [];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
      assertTrue(passwordsSection.$.menuExportPassword.hidden);
      done();
    });

    // Test that clicking the Export Passwords menu item opens the export
    // dialog.
    test('exportOpen', function(done) {
      const passwordList = [
        FakeDataMaker.passwordEntry('googoo.com', 'Larry', 1),
      ];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      // The export dialog calls requestExportProgressStatus() when opening.
      passwordManager.requestExportProgressStatus = (callback) => {
        callback(chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
        done();
      };
      passwordManager.addPasswordsFileExportProgressListener = () => {};
      passwordsSection.$.menuExportPassword.click();
    });

    // Test that tapping "Export passwords..." notifies the browser.
    test('startExport', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);

      passwordManager.exportPasswords = (callback) => {
        callback();
        done();
      };

      exportDialog.$$('#exportPasswordsButton').click();
    });

    // Test the export flow. If exporting is fast, we should skip the
    // in-progress view altogether.
    test('exportFlowFast', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const progressCallback = passwordManager.progressCallback;

      // Use this to freeze the delayed progress bar and avoid flakiness.
      const mockTimer = new MockTimer();
      mockTimer.install();

      assertTrue(exportDialog.$$('#dialog_start').open);
      exportDialog.$$('#exportPasswordsButton').click();
      assertTrue(exportDialog.$$('#dialog_start').open);
      progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
      progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});

      Polymer.dom.flush();
      // When we are done, the export dialog closes completely.
      assertFalse(!!exportDialog.$$('#dialog_start'));
      assertFalse(!!exportDialog.$$('#dialog_error'));
      assertFalse(!!exportDialog.$$('#dialog_progress'));
      done();

      mockTimer.uninstall();
    });

    // The error view is shown when an error occurs.
    test('exportFlowError', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const progressCallback = passwordManager.progressCallback;

      // Use this to freeze the delayed progress bar and avoid flakiness.
      const mockTimer = new MockTimer();
      mockTimer.install();

      assertTrue(exportDialog.$$('#dialog_start').open);
      exportDialog.$$('#exportPasswordsButton').click();
      assertTrue(exportDialog.$$('#dialog_start').open);
      progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
      progressCallback({
        status:
            chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
        folderName: 'tmp',
      });

      Polymer.dom.flush();
      // Test that the error dialog is shown.
      assertTrue(exportDialog.$$('#dialog_error').open);
      // Test that the error dialog can be dismissed.
      exportDialog.$$('#cancelErrorButton').click();
      Polymer.dom.flush();
      assertFalse(!!exportDialog.$$('#dialog_error'));
      done();

      mockTimer.uninstall();
    });

    // The error view allows to retry.
    test('exportFlowErrorRetry', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const progressCallback = passwordManager.progressCallback;
      // Use this to freeze the delayed progress bar and avoid flakiness.
      const mockTimer = new MockTimer();

      new Promise(resolve => {
        mockTimer.install();

        passwordManager.exportPasswords = resolve;
        exportDialog.$$('#exportPasswordsButton').click();
      }).then(() => {
        // This wait allows the BlockingRequestManager to process the click if
        // the test is running in ChromeOS.
        progressCallback(
            {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
        progressCallback({
          status:
              chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
          folderName: 'tmp',
        });

        Polymer.dom.flush();
        // Test that the error dialog is shown.
        assertTrue(exportDialog.$$('#dialog_error').open);
        // Test that clicking retry will start a new export.
        passwordManager.exportPasswords = (callback) => {
          callback();
          done();
        };
        exportDialog.$$('#tryAgainButton').click();

        mockTimer.uninstall();
      });
    });

    // Test the export flow. If exporting is slow, Chrome should show the
    // in-progress dialog for at least 1000ms.
    test('exportFlowSlow', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const progressCallback = passwordManager.progressCallback;

      const mockTimer = new MockTimer();
      mockTimer.install();

      // The initial dialog remains open for 100ms after export enters the
      // in-progress state.
      assertTrue(exportDialog.$$('#dialog_start').open);
      exportDialog.$$('#exportPasswordsButton').click();
      assertTrue(exportDialog.$$('#dialog_start').open);
      progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
      assertTrue(exportDialog.$$('#dialog_start').open);

      // After 100ms of not having completed, the dialog switches to the
      // progress bar. Chrome will continue to show the progress bar for 1000ms,
      // despite a completion event.
      mockTimer.tick(99);
      assertTrue(exportDialog.$$('#dialog_start').open);
      mockTimer.tick(1);
      Polymer.dom.flush();
      assertTrue(exportDialog.$$('#dialog_progress').open);
      progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
      assertTrue(exportDialog.$$('#dialog_progress').open);

      // After 1000ms, Chrome will display the completion event.
      mockTimer.tick(999);
      assertTrue(exportDialog.$$('#dialog_progress').open);
      mockTimer.tick(1);
      Polymer.dom.flush();
      // On SUCCEEDED the dialog closes completely.
      assertFalse(!!exportDialog.$$('#dialog_progress'));
      assertFalse(!!exportDialog.$$('#dialog_start'));
      assertFalse(!!exportDialog.$$('#dialog_error'));
      done();

      mockTimer.uninstall();
    });

    // Test that canceling the dialog while exporting will also cancel the
    // export on the browser.
    test('cancelExport', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const progressCallback = passwordManager.progressCallback;

      passwordManager.cancelExportPasswords = () => {
        done();
      };

      const mockTimer = new MockTimer();
      mockTimer.install();

      // The initial dialog remains open for 100ms after export enters the
      // in-progress state.
      exportDialog.$$('#exportPasswordsButton').click();
      progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
      // The progress bar only appears after 100ms.
      mockTimer.tick(100);
      Polymer.dom.flush();
      assertTrue(exportDialog.$$('#dialog_progress').open);
      exportDialog.$$('#cancel_progress_button').click();

      Polymer.dom.flush();
      // The dialog should be dismissed entirely.
      assertFalse(!!exportDialog.$$('#dialog_progress'));
      assertFalse(!!exportDialog.$$('#dialog_start'));
      assertFalse(!!exportDialog.$$('#dialog_error'));

      mockTimer.uninstall();
    });

    // The export dialog is dismissable.
    test('exportDismissable', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);

      assertTrue(exportDialog.$$('#dialog_start').open);
      exportDialog.$$('#cancelButton').click();
      Polymer.dom.flush();
      assertFalse(!!exportDialog.$$('#dialog_start'));

      done();
    });

    test('fires close event when canceled', () => {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const wait = test_util.eventToPromise(
          'passwords-export-dialog-close', exportDialog);
      exportDialog.$$('#cancelButton').click();
      return wait;
    });

    test('fires close event after export complete', () => {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const wait = test_util.eventToPromise(
          'passwords-export-dialog-close', exportDialog);
      exportDialog.$$('#exportPasswordsButton').click();
      passwordManager.progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
      passwordManager.progressCallback(
          {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
      return wait;
    });

    test('hideLinkToPasswordManagerWhenEncrypted', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      syncPrefs.encryptAllData = true;
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      sync_test_util.simulateSyncStatus({signedIn: true});
      Polymer.dom.flush();
      assertTrue(passwordsSection.$.manageLink.hidden);
    });

    test('showLinkToPasswordManagerWhenNotEncrypted', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      syncPrefs.encryptAllData = false;
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();
      assertFalse(passwordsSection.$.manageLink.hidden);
    });

    test('showLinkToPasswordManagerWhenNotSignedIn', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      sync_test_util.simulateSyncStatus({signedIn: false});
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();
      assertFalse(passwordsSection.$.manageLink.hidden);
    });

    test('leakDetectionToggleSignedOutWithFalsePref', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();

      passwordsSection.set(
          'prefs.profile.password_manager_leak_detection.value', false);
      sync_test_util.simulateSyncStatus({signedIn: false});
      sync_test_util.simulateStoredAccounts([]);
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();

      assertTrue(passwordsSection.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals(
          '', passwordsSection.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    test('leakDetectionToggleSignedOutWithTruePref', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();

      sync_test_util.simulateSyncStatus({signedIn: false});
      sync_test_util.simulateStoredAccounts([]);
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();

      assertTrue(passwordsSection.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals(
          loadTimeData.getString(
              'passwordsLeakDetectionSignedOutEnabledDescription'),
          passwordsSection.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    if (!cr.isChromeOS) {
      test('leakDetectionToggleSignedInNotSyncingWithFalsePref', function() {
        const passwordsSection =
            elementFactory.createPasswordsSection(passwordManager, [], []);
        const syncPrefs = sync_test_util.getSyncAllPrefs();

        passwordsSection.set(
            'prefs.profile.password_manager_leak_detection.value', false);
        sync_test_util.simulateSyncStatus({signedIn: false});
        sync_test_util.simulateStoredAccounts([
          {
            fullName: 'testName',
            givenName: 'test',
            email: 'test@test.com',
          },
        ]);
        cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
        Polymer.dom.flush();

        assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.disabled);
        assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.checked);
        assertEquals(
            '', passwordsSection.$.passwordsLeakDetectionCheckbox.subLabel);
      });

      test('leakDetectionToggleSignedInNotSyncingWithTruePref', function() {
        const passwordsSection =
            elementFactory.createPasswordsSection(passwordManager, [], []);
        const syncPrefs = sync_test_util.getSyncAllPrefs();

        sync_test_util.simulateSyncStatus({signedIn: false});
        sync_test_util.simulateStoredAccounts([
          {
            fullName: 'testName',
            givenName: 'test',
            email: 'test@test.com',
          },
        ]);
        cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
        Polymer.dom.flush();

        assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.disabled);
        assertTrue(passwordsSection.$.passwordsLeakDetectionCheckbox.checked);
        assertEquals(
            '', passwordsSection.$.passwordsLeakDetectionCheckbox.subLabel);
      });
    }

    test('leakDetectionToggleSignedInAndSyncingWithFalsePref', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();

      passwordsSection.set(
          'prefs.profile.password_manager_leak_detection.value', false);
      sync_test_util.simulateSyncStatus({signedIn: true});
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();

      assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals(
          '', passwordsSection.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    test('leakDetectionToggleSignedInAndSyncingWithTruePref', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();

      sync_test_util.simulateSyncStatus({signedIn: true});
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();

      assertFalse(passwordsSection.$.passwordsLeakDetectionCheckbox.disabled);
      assertTrue(passwordsSection.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals(
          '', passwordsSection.$.passwordsLeakDetectionCheckbox.subLabel);
    });
  });
});
