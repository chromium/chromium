// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager.create_profile_tests', function() {
  /** @return {!CreateProfileElement} */
  function createElement() {
    const createProfileElement = document.createElement('create-profile');
    document.body.appendChild(createProfileElement);
    return createProfileElement;
  }

  function registerTests() {
    /** @type {?TestProfileBrowserProxy} */
    let browserProxy = null;

    /** @type {?CreateProfileElement} */
    let createProfileElement = null;

    suite('CreateProfileTests', function() {
      setup(function() {
        browserProxy = new TestProfileBrowserProxy();

        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;
        browserProxy.setIcons([
          {url: 'icon1.png', label: 'icon1'}, {url: 'icon2.png', label: 'icon2'}
        ]);

        createProfileElement = createElement();

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function(done) {
        createProfileElement.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('Handles available profile icons', function() {
        return browserProxy.whenCalled('getAvailableIcons').then(function() {
          assertEquals(2, createProfileElement.availableIcons_.length);
        });
      });

      test('Name is empty by default', function() {
        assertEquals('', createProfileElement.$.nameInput.value);
      });

      test('Create button is disabled if name is empty or invalid', function() {
        assertEquals('', createProfileElement.$.nameInput.value);
        assertTrue(createProfileElement.$.save.disabled);

        createProfileElement.$.nameInput.value = ' ';
        assertTrue(createProfileElement.$.nameInput.invalid);
        assertTrue(createProfileElement.$.save.disabled);

        createProfileElement.$.nameInput.value = 'profile name';
        assertFalse(createProfileElement.$.nameInput.invalid);
        assertFalse(createProfileElement.$.save.disabled);
      });

      test('Create a profile', function() {
        // Create shortcut checkbox is invisible.
        const createShortcutCheckbox =
            createProfileElement.$.createShortcutCheckbox;
        assertTrue(createShortcutCheckbox.clientHeight == 0);

        // Enter a profile name.
        createProfileElement.$.nameInput.value = 'profile name';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('profile name', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertFalse(args.createShortcut);
        });
      });

      test('Leave the page by clicking the Cancel button', function() {
        return new Promise(function(resolve, reject) {
          // Create is not in progress. We expect to leave the page.
          createProfileElement.addEventListener('change-page', function(event) {
            if (event.detail.page == 'user-pods-page') {
              resolve();
            }
          });

          // Simulate clicking 'Cancel'.
          MockInteractions.tap(createProfileElement.$.cancel);
        });
      });

      test('Create profile success', function() {
        return new Promise(function(resolve, reject) {
          // Create was successful. We expect to leave the page.
          createProfileElement.addEventListener('change-page', function(event) {
            if (event.detail.page == 'user-pods-page') {
              resolve();
            }
          });

          // Enter a profile name.
          createProfileElement.$.nameInput.value = 'profile name';

          // Simulate clicking 'Create'.
          MockInteractions.tap(createProfileElement.$.save);

          browserProxy.whenCalled('createProfile').then(function(args) {
            // The paper-spinner is active when create is in progress.
            assertTrue(createProfileElement.createInProgress_);
            assertTrue(createProfileElement.$$('paper-spinner-lite').active);

            cr.webUIListenerCallback('create-profile-success');

            // The paper-spinner is not active when create is not in progress.
            assertFalse(createProfileElement.createInProgress_);
            assertFalse(createProfileElement.$$('paper-spinner-lite').active);
          });
        });
      });

      test('Create profile error', function() {
        // Enter a profile name.
        createProfileElement.$.nameInput.value = 'profile name';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          cr.webUIListenerCallback('create-profile-error', 'Error Message');

          // Create is no longer in progress.
          assertFalse(createProfileElement.createInProgress_);
          // Error message is set.
          assertEquals(
              'Error Message', createProfileElement.$.message.innerHTML);
        });
      });

      test('Create profile warning', function() {
        // Set the text in the name field.
        createProfileElement.$.nameInput.value = 'foo';

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          cr.webUIListenerCallback('create-profile-warning', 'Warning Message');

          // Create is no longer in progress.
          assertFalse(createProfileElement.createInProgress_);
          // Warning message is set.
          assertEquals(
              'Warning Message', createProfileElement.$.message.innerHTML);
        });
      });
    });

    suite('CreateProfileTestsNoSignedInUser', function() {
      setup(function() {
        browserProxy = new TestProfileBrowserProxy();
        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;

        browserProxy.setIcons([{url: 'icon1.png', label: 'icon1'}]);

        createProfileElement = createElement();

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function(done) {
        createProfileElement.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('Create button is disabled', function() {
        assertTrue(createProfileElement.$.save.disabled);
      });
    });

    suite('CreateProfileTestsProfileShortcutsEnabled', function() {
      setup(function() {
        browserProxy = new TestProfileBrowserProxy();
        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;
        browserProxy.setIcons([{url: 'icon1.png', label: 'icon1'}]);

        // Enable profile shortcuts feature.
        loadTimeData.overrideValues({
          profileShortcutsEnabled: true,
        });

        createProfileElement = createElement();
        // Enter a profile name.
        createProfileElement.$.nameInput.value = 'profile name';

        // Make sure DOM is up to date.
        Polymer.dom.flush();
      });

      teardown(function(done) {
        createProfileElement.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('Create profile without shortcut', function() {
        // Create shortcut checkbox is visible.
        const createShortcutCheckbox =
            createProfileElement.$.createShortcutCheckbox;
        assertTrue(createShortcutCheckbox.clientHeight > 0);

        // Create shortcut checkbox is checked.
        assertTrue(createShortcutCheckbox.checked);

        // Simulate unchecking the create shortcut checkbox.
        MockInteractions.tap(createShortcutCheckbox);

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('profile name', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertFalse(args.createShortcut);
        });
      });

      test('Create profile with shortcut', function() {
        // Create shortcut checkbox is visible.
        const createShortcutCheckbox =
            createProfileElement.$.createShortcutCheckbox;
        assertTrue(createShortcutCheckbox.clientHeight > 0);

        // Create shortcut checkbox is checked.
        assertTrue(createShortcutCheckbox.checked);

        // Simulate clicking 'Create'.
        MockInteractions.tap(createProfileElement.$.save);

        return browserProxy.whenCalled('createProfile').then(function(args) {
          assertEquals('profile name', args.profileName);
          assertEquals('icon1.png', args.profileIconUrl);
          assertTrue(args.createShortcut);
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
