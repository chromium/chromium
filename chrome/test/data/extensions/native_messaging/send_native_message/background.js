// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pinned SHA-256 fingerprint for ChromePublicTestSupport.apk.
const MATCHING_CERT =
    '32:A2:FC:74:D7:31:10:58:59:E5:A8:5D:F1:6D:95:F1:02:D8:5B:22' +
    ':09:9B:80:64:C5:D8:91:5C:61:DA:D1:E0';

chrome.runtime.sendNativeMessage(
    {
      application: 'org.chromium.chrome.tests.support',
      androidCertificates: [MATCHING_CERT],
    },
    {text: 'hello'}, (response) => {
      if (chrome.runtime.lastError) {
        chrome.test.sendMessage(`error: ${chrome.runtime.lastError.message}`);
      } else if (response && response.echo && response.echo.text === 'hello') {
        chrome.test.sendMessage(
            `app received isVerified: ${response.isVerified}`);
      } else {
        chrome.test.sendMessage(
            `unexpected response: ${JSON.stringify(response)}`);
      }
    });
