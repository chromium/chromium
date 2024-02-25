// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {maybeAutofillUsername} from 'chrome://chrome-signin/gaia_auth_host/saml_username_autofill.js';
import {appendParam} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

const IDP_URL_FOR_TESTS = 'https://login.corp.example.com/example';
const IDP_URL_FOR_TESTS_WITH_PARAMETER =
    'https://login.corp.example.com/example?name=value';
const EMAIL_FOR_TESTS = 'example@domain.com';

suite('SamlUsernameAutofillSuite', function() {
  // Test success for IdP url without parameters.
  test('SuccessNoParams', () => {
    const urlParameterNameToAutofillUsername = 'login_hint';
    const expectedResult = appendParam(
        IDP_URL_FOR_TESTS, urlParameterNameToAutofillUsername, EMAIL_FOR_TESTS);
    const newUrl = maybeAutofillUsername(
        IDP_URL_FOR_TESTS, urlParameterNameToAutofillUsername, EMAIL_FOR_TESTS);

    assertEquals(newUrl, expectedResult);
  });

  // Test success for IdP url with a parameter.
  test('SuccessWithParam', () => {
    const urlParameterNameToAutofillUsername = 'login_hint';
    const expectedResult = appendParam(
        IDP_URL_FOR_TESTS_WITH_PARAMETER, urlParameterNameToAutofillUsername,
        EMAIL_FOR_TESTS);
    const newUrl = maybeAutofillUsername(
        IDP_URL_FOR_TESTS_WITH_PARAMETER, urlParameterNameToAutofillUsername,
        EMAIL_FOR_TESTS);

    assertEquals(newUrl, expectedResult);
  });

  // Test that we do not add url parameter repeatedly.
  test('UrlAlreadyContainsParameter', () => {
    const urlParameterNameToAutofillUsername = 'login_hint';
    const urlWithAutofillParameter = appendParam(
        IDP_URL_FOR_TESTS, urlParameterNameToAutofillUsername, EMAIL_FOR_TESTS);
    const newUrl = maybeAutofillUsername(
        urlWithAutofillParameter, urlParameterNameToAutofillUsername,
        EMAIL_FOR_TESTS);

    assertEquals(newUrl, null);
  });

  // Test that our protection against repeated parameters isn't triggered if
  // url contains the same substring but not as a parameter.
  test('UrlCollisionWithParameterName', () => {
    const urlParameterNameToAutofillUsername = 'login_hint';
    // Make `login_hint` appear in IdP url, but not as a parameter.
    const IdpUrlCollidingWithParameterName = IDP_URL_FOR_TESTS + '/login_hint';
    const expectedResult = appendParam(
        IdpUrlCollidingWithParameterName, urlParameterNameToAutofillUsername,
        EMAIL_FOR_TESTS);
    const newUrl = maybeAutofillUsername(
        IdpUrlCollidingWithParameterName, urlParameterNameToAutofillUsername,
        EMAIL_FOR_TESTS);

    assertEquals(newUrl, expectedResult);
  });

  // Test that we don't autofill the username when user's email is absent.
  test('NoEmail', () => {
    const newUrl = maybeAutofillUsername(IDP_URL_FOR_TESTS, 'login_hint', '');

    assertEquals(newUrl, null);
  });

  // Test that we don't autofill the username when we don't know the appropriate
  // url parameter.
  test('NoUrlParameter', () => {
    const newUrl =
        maybeAutofillUsername(IDP_URL_FOR_TESTS, '', EMAIL_FOR_TESTS);
    assertEquals(newUrl, null);
  });

  // Test that we don't autofill the username when login page doesn't use https.
  test('HttpLoginPage', () => {
    const httpUrl = 'http://login.corp.example.com/example';
    const newUrl =
        maybeAutofillUsername(httpUrl, 'login_hint', EMAIL_FOR_TESTS);

    assertEquals(newUrl, null);
  });
});
