// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_browser_proxy.js';
import {EduCoexistenceController} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_controller.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_browser_proxy.js';

const FAKE_NOW_MILLISECONDS = 100000;
const FAKE_SIGNIN_TIME_MILLISECONDS = 50000;

suite('EduCoexistenceControllerTest', function() {
  let appComponent;
  let testBrowserProxy;
  let eduCoexistenceController: EduCoexistenceController;

  setup(function() {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setCoexistenceParams(getEduCoexistenceParams());

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // The controller wants an edu-coexistence-ui Polymer component
    // as a parameter.
    appComponent = document.createElement('edu-coexistence-ui');
    document.body.appendChild(appComponent);
    flush();

    eduCoexistenceController = new EduCoexistenceController(
        appComponent, document.createElement('webview'),
        getEduCoexistenceParams());
  });

  test('GetSigninTimeDelta', function() {
    // Fake Date.now()
    const realDateNow = Date.now;
    Date.now = () => {
      return FAKE_NOW_MILLISECONDS;
    };
    const expectedDeltaSeconds =
        (FAKE_NOW_MILLISECONDS - FAKE_SIGNIN_TIME_MILLISECONDS) / 1000;
    assertEquals(
        eduCoexistenceController.getTimeDeltaSinceSigninSeconds(),
        expectedDeltaSeconds);

    // Restore original Date.now()
    Date.now = realDateNow;
  });
});


function getEduCoexistenceParams() {
  return {
    url: 'https://foo.example.com/supervision/coexistence/intro',
    hl: 'en-US',
    sourceUi: 'oobe',
    clientId: 'test-client-id',
    clientVersion: ' test-client-version',
    eduCoexistenceId: ' test-edu-coexistence-id',
    platformVersion: ' test-platform-version',
    releaseChannel: 'test-release-channel',
    deviceId: 'test-device-id',
    signinTime: FAKE_SIGNIN_TIME_MILLISECONDS,
    eduCoexistenceAccessToken: 'token',
  };
}
