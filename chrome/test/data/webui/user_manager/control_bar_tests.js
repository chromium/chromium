// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager.control_bar_tests', function() {
  /** @return {!ControlBarElement} */
  function createElement() {
    const controlBarElement = document.createElement('control-bar');
    document.body.appendChild(controlBarElement);
    return controlBarElement;
  }

  function registerTests() {
    /** @type {?TestProfileBrowserProxy} */
    let browserProxy = null;

    /** @type {?ControlBarElement} */
    let controlBarElement = null;

    suite('ControlBarTests', function() {
      setup(function() {
        browserProxy = new TestProfileBrowserProxy();
        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;

        controlBarElement = createElement();
      });

      teardown(function(done) {
        controlBarElement.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('Actions are hidden by default', function() {
        assertTrue(controlBarElement.$.launchGuest.hidden);
        assertTrue(controlBarElement.$.addUser.hidden);

        controlBarElement.showGuest = true;
        controlBarElement.showAddPerson = true;
        Polymer.dom.flush();

        assertFalse(controlBarElement.$.launchGuest.hidden);
        assertFalse(controlBarElement.$.addUser.hidden);
      });

      test('Can create profile', function() {
        return new Promise(function(resolve, reject) {
          // We expect to go to the 'create-profile' page.
          listenOnce(controlBarElement, 'change-page', function(event) {
            if (event.detail.page == 'create-user-page') {
              resolve();
            }
          });

          // Simulate clicking 'Create Profile'.
          MockInteractions.tap(controlBarElement.$.addUser);
        });
      });

      test('Can launch guest profile', function() {
        // Simulate clicking 'Browse as guest'.
        MockInteractions.tap(controlBarElement.$.launchGuest);
        return browserProxy.whenCalled('launchGuestUser');
      });
    });

    suite('ControlBarTestsAllProfilesAreLocked', function() {
      /** @type {?ErrorDialogElement} */
      let errorDialogElement = null;

      setup(function() {
        browserProxy = new TestProfileBrowserProxy();
        // Replace real proxy with mock proxy.
        signin.ProfileBrowserProxyImpl.instance_ = browserProxy;

        browserProxy.setAllProfilesLocked(true);

        controlBarElement = createElement();

        errorDialogElement = document.querySelector('error-dialog');
      });

      teardown(function(done) {
        controlBarElement.remove();
        if (errorDialogElement.$.dialog.open) {
          errorDialogElement.$.dialog.close();
        }

        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('Cannot create profile', function() {
        // Simulate clicking 'Create Profile'.
        MockInteractions.tap(controlBarElement.$.addUser);

        return browserProxy.whenCalled('areAllProfilesLocked').then(function() {
          // Make sure DOM is up to date.
          Polymer.dom.flush();

          // The dialog is visible.
          assertTrue(errorDialogElement.$.dialog.open);
        });
      });

      test('Cannot launch guest profile', function() {
        // Simulate clicking 'Browse as guest'.
        MockInteractions.tap(controlBarElement.$.launchGuest);

        return browserProxy.whenCalled('areAllProfilesLocked').then(function() {
          // Make sure DOM is up to date.
          Polymer.dom.flush();

          // The error dialog is visible.
          assertTrue(errorDialogElement.$.dialog.open);
        });
      });

      test('Can create profile with force signin', function() {
        controlBarElement.isForceSigninEnabled_ = true;
        Polymer.dom.flush();
        return new Promise(function(resolve, reject) {
          // We expect to go to the 'create-profile' page.
          listenOnce(controlBarElement, 'change-page', function(event) {
            if (event.detail.page == 'create-user-page') {
              resolve();
            }
          });

          // Simulate clicking 'Create Profile'.
          MockInteractions.tap(controlBarElement.$.addUser);
        });
      });

      test('Can launch guest profile with force sign in', function() {
        controlBarElement.isForceSigninEnabled_ = true;
        Polymer.dom.flush();
        MockInteractions.tap(controlBarElement.$.launchGuest);
        return browserProxy.whenCalled('launchGuestUser');
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
