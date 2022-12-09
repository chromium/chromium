// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {detectPasswordChangeSuccess} from 'chrome://chrome-signin/gaia_auth_host/password_change_authenticator.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

const EXAMPLE_ADFS_ENDPOINT = 'https://example.com/adfs/portal/updatepassword/';

const EXAMPLE_AZURE_ENDPOINT =
    'https://example.windowsazure.com/ChangePassword.aspx';

const EXAMPLE_OKTA_ENDPOINT =
    'https://example.okta.com/user/profile/internal_login/password';

const EXAMPLE_PING_ENDPOINT =
    'https://login.pingone.com/idp/directory/a/12345/password/chg/67890';


function assertSuccess(postUrl, redirectUrl) {
  assertTrue(detectSuccess(postUrl, redirectUrl));
}

function assertNotSuccess(postUrl, redirectUrl) {
  assertFalse(detectSuccess(postUrl, redirectUrl));
}

function detectSuccess(postUrl, redirectUrl) {
  postUrl = (typeof postUrl === 'string') ? new URL(postUrl) : postUrl;
  redirectUrl =
      (typeof redirectUrl === 'string') ? new URL(redirectUrl) : redirectUrl;
  return detectPasswordChangeSuccess(postUrl, redirectUrl);
}

suite('PasswordChangeAuthenticatorSuite', function() {
  test('DetectAdfsSuccess', () => {
    const endpointUrl = EXAMPLE_ADFS_ENDPOINT;

    assertNotSuccess(endpointUrl, endpointUrl);
    assertNotSuccess(endpointUrl, endpointUrl + '?status=1');

    assertSuccess(endpointUrl, endpointUrl + '?status=0');
    assertSuccess(endpointUrl + '?status=1', endpointUrl + '?status=0');

    // We allow "status=0" to count as success everywhere right now, but this
    // should be narrowed down to ADFS - see the TODO in the code.
    assertSuccess(EXAMPLE_AZURE_ENDPOINT, EXAMPLE_AZURE_ENDPOINT + '?status=0');
  });

  test('DetectAzureSuccess', () => {
    const endpointUrl = EXAMPLE_AZURE_ENDPOINT;
    const extraParam = 'BrandContextID=O123';

    assertNotSuccess(endpointUrl, endpointUrl);
    assertNotSuccess(endpointUrl, endpointUrl + '?' + extraParam);
    assertNotSuccess(endpointUrl, endpointUrl + '?ReturnCode=1&' + extraParam);
    assertNotSuccess(
        endpointUrl, endpointUrl + '?' + extraParam + '&ReturnCode=1');
    assertNotSuccess(EXAMPLE_PING_ENDPOINT, endpointUrl + '?ReturnCode=0');

    assertSuccess(endpointUrl, endpointUrl + '?ReturnCode=0');
    assertSuccess(
        endpointUrl + '?' + extraParam,
        endpointUrl + '?ReturnCode=0&' + extraParam);
    assertSuccess(
        endpointUrl + '?' + extraParam,
        endpointUrl + '?' + extraParam + '&ReturnCode=0');
  });

  test('DetectPingSuccess', () => {
    const endpointUrl = EXAMPLE_PING_ENDPOINT;

    assertNotSuccess(endpointUrl, endpointUrl);
    assertNotSuccess(
        endpointUrl + '?returnurl=https://desktop.pingone.com',
        endpointUrl + '?returnurl=https://desktop.pingone.com');
    assertNotSuccess(
        endpointUrl, endpointUrl + '?returnurl=https://desktop.pingone.com');

    assertSuccess(
        endpointUrl + '?returnurl=https://desktop.pingone.com',
        'https://desktop.pingone.com/Selection?cmd=selection');
  });
});
