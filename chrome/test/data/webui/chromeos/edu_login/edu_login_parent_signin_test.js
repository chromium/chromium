// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_login_parent_signin.js';
import 'chrome://chrome-signin/edu_login_button.js';

import {EduAccountLoginBrowserProxyImpl} from 'chrome://chrome-signin/browser_proxy.js';
import {ParentAccount} from 'chrome://chrome-signin/edu_login_util.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFakeParent, TestEduAccountLoginBrowserProxy} from './edu_login_test_util.js';

window.edu_login_parent_signin_tests = {};
edu_login_parent_signin_tests.suiteName = 'EduLoginParentSigninTest';

/** @enum {string} */
edu_login_parent_signin_tests.TestNames = {
  Initialize: 'Check page state on init',
  WrongPassword: 'State after wrong password error',
  ParentSigninSuccess: 'State after signin success',
  ShowHidePassword: 'Icon shows/hides the password',
  ClearState: 'State is cleared after back button click',
};

/** @type {ParentAccount} */
const fakeParent =
    getFakeParent('parent1@gmail.com', 'Parent 1', '', 'parent1gaia');

suite(edu_login_parent_signin_tests.suiteName, function() {
  let parentSigninComponent;
  let testBrowserProxy;
  let passwordField;

  /** @param {string} type */
  function clickButton(type) {
    parentSigninComponent.$$(`edu-login-button[button-type="${type}"]`)
        .$$('cr-button')
        .click();
  }

  setup(function() {
    testBrowserProxy = new TestEduAccountLoginBrowserProxy();
    EduAccountLoginBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    parentSigninComponent = document.createElement('edu-login-parent-signin');
    parentSigninComponent.parent = fakeParent;
    document.body.appendChild(parentSigninComponent);
    flush();
    passwordField = parentSigninComponent.$$('#passwordField');
  });

  teardown(function() {
    parentSigninComponent.remove();
  });

  test(assert(edu_login_parent_signin_tests.TestNames.Initialize), function() {
    assertEquals(
        loadTimeData.getStringF('parentSigninTitle', fakeParent.displayName),
        parentSigninComponent.$$('h1').textContent.trim());
    assertFalse(passwordField.classList.contains('error'));
    assertEquals('password', passwordField.type);
    assertEquals(
        'edu-login-icons:show-password-icon',
        parentSigninComponent.$$('.password-icon').getAttribute('iron-icon'));
    assertTrue(parentSigninComponent.$$('edu-login-button[button-type="next"]')
                   .disabled);
    assertTrue(parentSigninComponent.$$('.error-message').hidden);
  });

  test(
      assert(edu_login_parent_signin_tests.TestNames.WrongPassword),
      function() {
        testBrowserProxy.setParentSigninResponse(
            () => Promise.reject({isWrongPassword: true}));

        parentSigninComponent.password_ = 'fake-password';
        assertFalse(
            parentSigninComponent.$$('edu-login-button[button-type="next"]')
                .disabled);
        clickButton('next');
        testBrowserProxy.whenCalled('parentSignin').then(function() {
          assertTrue(passwordField.classList.contains('error'));
          assertFalse(parentSigninComponent.$$('.error-message').hidden);
        });
      });

  test(
      assert(edu_login_parent_signin_tests.TestNames.ParentSigninSuccess),
      function() {
        let goNextCalls = 0;
        parentSigninComponent.addEventListener('go-next', function() {
          goNextCalls++;
        });
        testBrowserProxy.setParentSigninResponse(
            () => Promise.resolve('fake-rapt'));

        parentSigninComponent.password_ = 'fake-password';
        assertFalse(
            parentSigninComponent.$$('edu-login-button[button-type="next"]')
                .disabled);
        assertEquals(0, goNextCalls);
        clickButton('next');
        testBrowserProxy.whenCalled('parentSignin').then(function() {
          assertEquals(1, goNextCalls);
          assertFalse(passwordField.classList.contains('error'));
          assertTrue(parentSigninComponent.$$('.error-message').hidden);
        });
      });

  test(
      assert(edu_login_parent_signin_tests.TestNames.ShowHidePassword),
      function() {
        const passwordIcon = parentSigninComponent.$$('.password-icon');
        assertEquals('password', passwordField.type);
        assertEquals(
            'edu-login-icons:show-password-icon',
            passwordIcon.getAttribute('iron-icon'));
        passwordIcon.click();
        assertEquals('text', passwordField.type);
        assertEquals(
            'edu-login-icons:hide-password-icon',
            passwordIcon.getAttribute('iron-icon'));
      });

  test(assert(edu_login_parent_signin_tests.TestNames.ClearState), function() {
    testBrowserProxy.setParentSigninResponse(
        () => Promise.reject({isWrongPassword: true}));
    parentSigninComponent.password_ = 'fake-password';
    clickButton('next');
    testBrowserProxy.whenCalled('parentSignin').then(function() {
      clickButton('back');
      assertEquals('', passwordField.value);
      assertFalse(passwordField.classList.contains('error'));
      assertTrue(parentSigninComponent.$$('.error-message').hidden);
    });
  });
});
