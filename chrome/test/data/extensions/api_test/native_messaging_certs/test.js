// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(async function(config) {
  const APP_NAME = 'org.chromium.chrome.tests.support';
  const EXPECTED_ERROR = `Unable to connect to ${APP_NAME}.`;

  // Pinned SHA-256 fingerprint for ChromePublicTestSupport.apk.
  const MATCHING_CERT =
      '32:A2:FC:74:D7:31:10:58:59:E5:A8:5D:F1:6D:95:F1:02:D8:5B:22' +
      ':09:9B:80:64:C5:D8:91:5C:61:DA:D1:E0';

  // A SHA-256 fingerprint that does not match ChromePublicTestSupport.apk.
  const NON_MATCHING_CERT =
      'FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF' +
      ':FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF';

  chrome.test.runTests([
    // Messaging with the valid pinned certificate works.
    async function matchingCertificate() {
      const message = {text: 'matching certificate test'};
      const response = await chrome.runtime.sendNativeMessage(
          {application: APP_NAME, androidCertificates: [MATCHING_CERT]},
          message);

      chrome.test.assertEq(message, response.echo);
      chrome.test.succeed();
    },

    // Messaging with a valid format but non-matching certificate fails.
    async function nonMatchingCertificate() {
      const message = {text: 'non-matching certificate test'};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage(
              {application: APP_NAME, androidCertificates: [NON_MATCHING_CERT]},
              message),
          `Error: ${EXPECTED_ERROR}`);
      chrome.test.succeed();
    },

    // Messaging with multiple certificates where at least one matches works.
    async function multipleCertificatesWithOneMatching() {
      const message = {text: 'multi-cert with one matching test'};
      const response = await chrome.runtime.sendNativeMessage(
          {
            application: APP_NAME,
            androidCertificates: [NON_MATCHING_CERT, MATCHING_CERT],
          },
          message);

      chrome.test.assertEq(message, response.echo);
      chrome.test.succeed();
    },
  ]);
});
