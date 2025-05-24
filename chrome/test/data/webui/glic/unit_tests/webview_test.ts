// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {matcherForOrigin, urlMatchesAllowedOrigin} from 'chrome://glic/webview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('WebviewTest', () => {
  setup(() => {
    loadTimeData.resetForTesting({
      glicAllowedOrigins: '',
      glicGuestURL: 'https://cat.fun/',
      devMode: false,
    });
  });

  function assertUrlMatchesAllowedOrigin(expectMatches: boolean, url: string) {
    assertEquals(
        expectMatches, urlMatchesAllowedOrigin(url),
        `urlMatchesAllowedOrigin("${url}")`);
  }

  test('matcherForOrigin works', () => {
    assertFalse(!!matcherForOrigin(''));
    assertFalse(!!matcherForOrigin('fun'));
    assertFalse(!!matcherForOrigin('cat.fun'));

    let result = matcherForOrigin('https://cat.fun');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('https', result?.protocol);

    result = matcherForOrigin('http://cat.fun');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);

    result = matcherForOrigin('http://cat.fun:42');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
    assertEquals('42', result?.port);

    result = matcherForOrigin('http://cat.fun:*');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
    assertEquals('*', result?.port);

    assertFalse(!!matcherForOrigin('http://cat.fun/://foo://'));
  });

  test('urlMatchesAllowedOrigin allows the primary url', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: '',
      glicGuestURL: 'https://cat.fun/party',
    });
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/party');
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/disaster');
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/');
    assertUrlMatchesAllowedOrigin(false, 'https://dog.fun/');
    assertUrlMatchesAllowedOrigin(false, 'http://cat.fun/');
  });

  test('urlMatchesAllowedOrigin allows allowed origins', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: 'https://*.mouse.org https://dog.com',
      glicGuestURL: 'https://cat.fun/party',
    });

    assertUrlMatchesAllowedOrigin(true, 'https://sub.mouse.org/party');
    assertUrlMatchesAllowedOrigin(true, 'https://inner.sub.mouse.org/party');
    assertUrlMatchesAllowedOrigin(false, 'https://mouse.org');
    assertUrlMatchesAllowedOrigin(false, 'https://amouse.org');

    assertUrlMatchesAllowedOrigin(true, 'https://dog.com/party');
    assertUrlMatchesAllowedOrigin(true, 'https://dog.com:99/party');
    assertUrlMatchesAllowedOrigin(false, 'http://dog.com/party');
  });

  test('urlMatchesAllowedOrigin allows http', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: '',
      glicGuestURL: 'http://test.com',
    });

    assertUrlMatchesAllowedOrigin(true, 'http://test.com');
    assertUrlMatchesAllowedOrigin(false, 'https://test.com');
    assertUrlMatchesAllowedOrigin(false, 'http://other.com');
  });
});
