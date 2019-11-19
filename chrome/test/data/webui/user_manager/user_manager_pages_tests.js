// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager.user_manager_pages_tests', function() {
  function registerTests() {
    suite('UserManagerPagesTests', function() {
      /** @type {?UserManagerPagesElement} */
      let pagesElement = null;

      setup(function() {
        pagesElement = document.createElement('user-manager-pages');
        document.body.appendChild(pagesElement);
      });

      teardown(function(done) {
        pagesElement.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('User Pods page is the default visible page', function() {
        const activeView =
            pagesElement.shadowRoot.querySelector('div.active[slot="view"]');
        assertEquals('user-pods-page', activeView.id);
      });

      test('Change page listener works', function() {
        assertEquals('user-pods-page', pagesElement.selectedPage_);
        pagesElement.fire('change-page', {page: 'create-user-page'});
        assertEquals('create-user-page', pagesElement.selectedPage_);
      });

      test('Create profile page gets restamped', function() {
        /** @type {?CreateProfileElement} */
        let createProfileElement = null;

        // Not initially in the DOM.
        createProfileElement = pagesElement.$$('create-profile');
        assertTrue(!createProfileElement);

        pagesElement.fire('change-page', {page: 'create-user-page'});
        Polymer.dom.flush();

        // Present in the DOM.
        createProfileElement = pagesElement.$$('create-profile');
        assertTrue(!!createProfileElement);

        pagesElement.fire('change-page', {page: 'user-pods-page'});
        Polymer.dom.flush();

        // Not present in the DOM.
        createProfileElement = pagesElement.$$('create-profile');
        assertTrue(!createProfileElement);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
