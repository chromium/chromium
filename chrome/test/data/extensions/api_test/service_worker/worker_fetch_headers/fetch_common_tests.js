// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a common set of `fetch()` header test cases.
 */
export function getCommonFetchHeaderTests() {
  return [

    /**
     * Tests calling fetch() and attempting to set a non-forbidden header.
     *
     * Note: this test requires that chrome.test.getConfig it setup before the
     * test is run.
     */
    async function fetchWithoutSettingForbiddenHeader() {
      chrome.test.getConfig(function(config) {
        var url = 'http://127.0.0.1:' + config.testServer.port +
            '/fetch/fetch_allowed.html';
        fetch(url, {
          method: 'GET',
          headers: {
            // Content-Type is not a forbidden header and can be set.
            'Content-Type': 'text/testing',
          },
        })
            .then(() => {
              chrome.test.succeed();
            })
            .catch((e) => {
              chrome.test.fail(e);
            });
      });
    },

    /**
     * Tests calling fetch() and attempting to set a forbidden header.
     *
     * Note: this test requires that chrome.test.getConfig it setup before the
     * test is run.
     */
    async function fetchAndSetForbiddenHeader() {
      chrome.test.getConfig(function(config) {
        var url = 'http://127.0.0.1:' + config.testServer.port +
            '/fetch/fetch_forbidden.html';
        fetch(url, {
          method: 'GET',
          headers: {
            // Accept-Encoding is a forbidden header:
            // https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_request_header
            'Accept-Encoding': 'fakeencoding, fakeencoding2',
          },
        })
            .then(() => {
              chrome.test.succeed();
            })
            .catch((e) => {
              chrome.test.fail(e);
            });
      });
    }

  ];
}
