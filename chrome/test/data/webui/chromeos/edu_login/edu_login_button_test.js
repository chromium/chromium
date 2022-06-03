// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_login_button.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

window.edu_login_button_tests = {};
edu_login_button_tests.suiteName = 'EduLoginButtonTest';

/** @enum {string} */
edu_login_button_tests.TestNames = {
  OkButtonProperties: 'OK button properties',
  NextButtonProperties: 'Next button properties',
  BackButtonProperties: 'Back button properties',
  OkButtonRtlIcon: 'OK button RTL icon',
  NextButtonRtlIcon: 'Next button RTL icon',
  BackButtonRtlIcon: 'Back button RTL icon',
};

/** @param {boolean} isRTL */
function finishSetup(isRTL) {
  document.documentElement.dir = isRTL ? 'rtl' : 'ltr';
  flush();
}

suite(edu_login_button_tests.suiteName, function() {
  let okButton;
  let nextButton;
  let backButton;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <edu-login-button button-type="ok" id="okButton"></edu-login-button>
      <edu-login-button button-type="next" id="nextButton" disabled>
      </edu-login-button>
      <edu-login-button button-type="back" id="backButton"></edu-login-button>
    `;
    okButton = document.body.querySelector('#okButton');
    nextButton = document.body.querySelector('#nextButton');
    backButton = document.body.querySelector('#backButton');
  });

  test(assert(edu_login_button_tests.TestNames.OkButtonProperties), function() {
    finishSetup(false);
    assertEquals('ok', okButton.buttonType);
    assertFalse(okButton.disabled);
    assertEquals(1, okButton.$$('cr-button').classList.length);
    assertEquals('action-button', okButton.$$('cr-button').classList[0]);
    assertEquals(
        loadTimeData.getString('okButton'),
        okButton.$$('cr-button').textContent.trim());
    // OK button shouldn't have icon.
    assertEquals(null, okButton.$$('iron-icon'));
  });

  test(
      assert(edu_login_button_tests.TestNames.NextButtonProperties),
      function() {
        finishSetup(false);
        assertEquals('next', nextButton.buttonType);
        assertTrue(nextButton.disabled);
        assertEquals(1, nextButton.$$('cr-button').classList.length);
        assertEquals('action-button', nextButton.$$('cr-button').classList[0]);
        assertEquals(
            loadTimeData.getString('nextButton'),
            nextButton.$$('cr-button').textContent.trim());
        assertEquals('cr:chevron-right', nextButton.$$('iron-icon').icon);
      });

  test(
      assert(edu_login_button_tests.TestNames.BackButtonProperties),
      function() {
        finishSetup(false);
        assertEquals('back', backButton.buttonType);
        assertFalse(backButton.disabled);
        assertEquals(0, backButton.$$('cr-button').classList.length);
        assertEquals(
            loadTimeData.getString('backButton'),
            backButton.$$('cr-button').textContent.trim());
        assertEquals('cr:chevron-left', backButton.$$('iron-icon').icon);
      });

  test(assert(edu_login_button_tests.TestNames.OkButtonRtlIcon), function() {
    finishSetup(true);
    // OK button shouldn't have icon.
    assertEquals(null, okButton.$$('iron-icon'));
  });

  test(assert(edu_login_button_tests.TestNames.NextButtonRtlIcon), function() {
    finishSetup(true);
    assertEquals('cr:chevron-left', nextButton.$$('iron-icon').icon);
  });

  test(assert(edu_login_button_tests.TestNames.BackButtonRtlIcon), function() {
    finishSetup(true);
    assertEquals('cr:chevron-right', backButton.$$('iron-icon').icon);
  });
});
