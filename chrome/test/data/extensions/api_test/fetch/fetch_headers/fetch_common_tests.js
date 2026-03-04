// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns a common set of `fetch()` header test cases.
export function getCommonFetchHeaderTests() {
  let config;
  const getUrl = (path) => `http://127.0.0.1:${config.testServer.port}${path}`;

  return [

    async function setUp() {
      config = await chrome.test.getConfig();
      chrome.test.succeed();
    },

    // Tests calling fetch() and attempting to set a non-forbidden header
    // directly on the fetch request.
    async function fetchWithoutSettingForbiddenHeader() {
      const url = getUrl('/fetch_allowed.html');

      await fetch(url, {
        method: 'GET',
        headers: {
          // Content-Type is not a forbidden header and can be set.
          'Content-Type': 'text/testing',
        },
      });
      chrome.test.succeed();
    },

    // Tests calling fetch() and attempting to set the forbidden Origin header
    // directly on the fetch request. If the bypass is not enabled or allowed
    // for this context, the fetch should still succeed but silently drop the
    // header.
    async function fetchAndSetForbiddenOriginHeader() {
      const url = getUrl('/fetch_forbidden.html?test=fetchAndSet');

      await fetch(url, {
        method: 'GET',
        headers: {
          // Origin is a forbidden header:
          // https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_request_header
          'Origin': 'fakescheme://fakehostname',
        },
      });
      chrome.test.succeed();
    },

    // Tests calling fetch() with a POST request and attempting to set a
    // forbidden header directly on the request. This ensures the network
    // service does not clobber the custom Origin header on non-GET requests.
    async function fetchPostAndSetForbiddenHeader() {
      const url = getUrl('/fetch_forbidden.html?test=fetchPostAndSet');

      await fetch(url, {
        method: 'POST',
        headers: {
          'Origin': 'fakescheme://fakehostname',
        },
        body: 'hello',
      });
      chrome.test.succeed();
    },

    // Tests setting a forbidden header via the Headers API. This ensure the
    // Blink layer will allow a Headers API caller to set the Origin forbidden
    // header successfully. The forbidden header will remain on the fetch
    // request after leaving the renderer.
    async function headersApiSetForbiddenHeader() {
      const url = getUrl('/fetch_forbidden.html?test=headersApiSet');

      const req = new Request(url, {method: 'GET'});
      req.headers.set('Origin', 'fakescheme://fakehostname');

      // If the bypass is not active (e.g. feature is disabled, or this is
      // running in a content script), the Fetch spec dictates that set()
      // silently fails, so get() will return null. If the bypass is active, it
      // will return our custom value.
      const origin = req.headers.get('Origin');
      if (origin !== null) {
        chrome.test.assertEq('fakescheme://fakehostname', origin);
      }

      await fetch(req);
      chrome.test.succeed();
    },

    // Tests removing a forbidden header via the Headers API. This ensures the
    // Blink layer will silently ignore a caller attempting to modify the header
    // this way. The original modified Origin header will remain on the fetch
    // request after leaving the renderer.
    async function headersApiRemoveForbiddenHeader() {
      const url = getUrl('/fetch_forbidden.html?test=headersApiRemove');

      const req = new Request(url, {
        method: 'GET',
        headers: {'Origin': 'fakescheme://fakehostname'},
      });

      const originBefore = req.headers.get('Origin');
      req.headers.delete('Origin');

      // Even if the bypass is active for setting the header, deleting a
      // forbidden header is still blocked by the Fetch spec. So delete()
      // silently fails and the value should remain unchanged.
      const originAfter = req.headers.get('Origin');
      chrome.test.assertEq(originBefore, originAfter);

      await fetch(req);
      chrome.test.succeed();
    },
  ];
}
