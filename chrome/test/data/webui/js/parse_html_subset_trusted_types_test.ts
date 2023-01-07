// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {parseHtmlSubset} from 'chrome://resources/js/parse_html_subset.js';
import {assertNotReached} from 'chrome://webui-test/chai_assert.js';

suite('ParseHtmlSubsetTrustedTypesTest', function() {
  test('parseHtmlSubset won\'t cause Trusted Types violation', () => {
    const meta = document.createElement('meta');
    meta.httpEquiv = 'content-security-policy';
    meta.content = 'require-trusted-types-for \'script\';';
    document.head.appendChild(meta);

    try {
      parseHtmlSubset('<b>bold</b>');
    } catch (e) {
      assertNotReached('Trusted Types violation detected');
    }
  });
});
