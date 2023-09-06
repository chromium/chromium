// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

declare global {
  interface Window {
    // Exported by the prod code.
    whenPageIsPopulatedForTest(): Promise<void>;
  }
}

suite('MediaEngagement', function() {
  const EXAMPLE_URL_1 = 'http://example.com';
  const EXAMPLE_URL_2 = 'http://shmlexample.com';

  suiteSetup(function() {
    return window.whenPageIsPopulatedForTest();
  });

  test('check engagement values are loaded', function() {
    const originCells =
        Array.from(document.body.querySelectorAll('.origin-cell'));
    assertDeepEquals(
        [EXAMPLE_URL_1, EXAMPLE_URL_2], originCells.map(x => x.textContent));
  });
});
