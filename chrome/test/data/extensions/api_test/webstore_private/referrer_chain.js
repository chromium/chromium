// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function getReferrerChain() {
    chrome.webstorePrivate.getReferrerChain((result) => {
      // |result| contains the base64-encoded referrer data. Since the data
      // contains the (randomly generated) server port, we cannot simply
      // compare it to an expected base64-encoded string.
      var referrerString = atob(result);
      chrome.test.assertTrue(
          referrerString.includes('example.com'),
          'Referrer data should contain example.com');
      chrome.test.assertTrue(
          referrerString.includes('redirect1.com'),
          'Referrer data should contain redirect1.com');
      chrome.test.assertTrue(
          referrerString.includes('redirect2.com'),
          'Referrer data should contain redirect2.com');
      chrome.test.succeed();
    });
  },
];

chrome.test.runTests(tests);
