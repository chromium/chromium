// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_login_parents.js';

import {EduAccountLoginBrowserProxyImpl} from 'chrome://chrome-signin/browser_proxy.js';
import {EduLoginErrorType, ParentAccount} from 'chrome://chrome-signin/edu_login_util.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFakeParent, getFakeParentsList, TestEduAccountLoginBrowserProxy} from './edu_login_test_util.js';

window.edu_login_parents_tests = {};
edu_login_parents_tests.suiteName = 'EduLoginParentsTest';

/** @enum {string} */
edu_login_parents_tests.TestNames = {
  Initialize: 'Expect getParents call on initialize',
  NextButton: 'Next button is enabled only when parent is selected',
  GoNext: 'go-next event should be fired when parent is selected',
  SelectedParent: 'Selected parent',
  NoInternetError: 'No internet error',
  CannotAddAccountError: 'Cannot add account error',
};

suite(edu_login_parents_tests.suiteName, function() {
  let parentsComponent;
  let testBrowserProxy;

  /** @return {NodeList} */
  function getAccountListItems() {
    return parentsComponent.shadowRoot.querySelectorAll('.account-list-item');
  }

  /**
   * @param {boolean} isNetworkReady
   * @param {function} getParentsResponse
   */
  function finishSetup(isNetworkReady = true, getParentsResponse = null) {
    testBrowserProxy.setIsNetworkReadyResponse(
        () => Promise.resolve(isNetworkReady));
    if (getParentsResponse) {
      testBrowserProxy.setGetParentsResponse(getParentsResponse);
    }

    parentsComponent = document.createElement('edu-login-parents');
    document.body.appendChild(parentsComponent);
    flush();
  }

  setup(function() {
    PolymerTest.clearBody();
    testBrowserProxy = new TestEduAccountLoginBrowserProxy();
    EduAccountLoginBrowserProxyImpl.instance_ = testBrowserProxy;
  });

  test(assert(edu_login_parents_tests.TestNames.Initialize), function() {
    finishSetup();
    assertEquals(1, testBrowserProxy.getCallCount('isNetworkReady'));
  });

  test(assert(edu_login_parents_tests.TestNames.NextButton), function() {
    finishSetup();
    testBrowserProxy.whenCalled('isNetworkReady').then(function() {
      testBrowserProxy.whenCalled('getParents').then(function() {
        flush();
        assertTrue(parentsComponent.$$('edu-login-button[button-type="next"]')
                       .disabled);
        // Select the first parent from the list.
        getAccountListItems()[0].click();
        assertFalse(parentsComponent.$$('edu-login-button[button-type="next"]')
                        .disabled);
      });
    });
  });

  test(assert(edu_login_parents_tests.TestNames.GoNext), function() {
    finishSetup();
    let goNextCalls = 0;
    parentsComponent.addEventListener('go-next', function() {
      goNextCalls++;
    });
    testBrowserProxy.whenCalled('isNetworkReady').then(function() {
      testBrowserProxy.whenCalled('getParents').then(function() {
        flush();
        assertEquals(0, goNextCalls);
        const accountListItems = getAccountListItems();
        // Select the first parent from the list.
        accountListItems[0].click();
        assertEquals(1, goNextCalls);
        // If user goes back and selects the same item -'go-next' should be
        // fired.
        accountListItems[0].click();
        assertEquals(2, goNextCalls);
        // If user goes back and selects another item - 'go-next' should be
        // fired.
        accountListItems[1].click();
        assertEquals(3, goNextCalls);
      });
    });
  });

  test(assert(edu_login_parents_tests.TestNames.SelectedParent), function() {
    finishSetup();
    testBrowserProxy.whenCalled('isNetworkReady').then(function() {
      testBrowserProxy.whenCalled('getParents').then(function() {
        flush();
        assertDeepEquals(getFakeParentsList(), parentsComponent.parents_);
        assertEquals(null, parentsComponent.selectedParent);
        // Select the first parent from the list.
        getAccountListItems()[0].click();
        assertDeepEquals(
            getFakeParentsList()[0], parentsComponent.selectedParent);
      });
    });
  });

  test(assert(edu_login_parents_tests.TestNames.NoInternetError), function() {
    finishSetup(/*isNetworkReady=*/ false);
    let errorCalls = 0;
    let errorType = '';
    parentsComponent.addEventListener('edu-login-error', (e) => {
      errorCalls++;
      errorType = e.detail.errorType;
    });
    testBrowserProxy.whenCalled('isNetworkReady').then(function() {
      assertEquals(1, errorCalls);
      assertEquals(EduLoginErrorType.NO_INTERNET, errorType);
    });
  });

  test(
      assert(edu_login_parents_tests.TestNames.CannotAddAccountError),
      function() {
        finishSetup(
            /*isNetworkReady=*/ true,
            /*getParentsResponse=*/ () => Promise.reject());
        let errorCalls = 0;
        let errorType = '';
        parentsComponent.addEventListener('edu-login-error', (e) => {
          errorCalls++;
          errorType = e.detail.errorType;
        });
        testBrowserProxy.whenCalled('isNetworkReady').then(function() {
          testBrowserProxy.whenCalled('getParents').then(function() {
            assertEquals(1, errorCalls);
            assertEquals(EduLoginErrorType.CANNOT_ADD_ACCOUNT, errorType);
          });
        });
      });
});
