// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that the JS file is loaded and the test is running.
suite('ViewerJs', function() {
  test('SanitizeLinks', async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    container.innerHTML = '<a href="http://example.com">good link</a>' +
        '<a href="javascript:alert(1)">bad link</a>' +
        '<a href="ftp://example.com">another bad link</a>';
    document.body.appendChild(container);

    sanitizeLinks(container);

    assert.equal(
        container.innerHTML,
        '<a href="http://example.com" target="_blank">good link</a>' +
            'bad linkanother bad link');
  });
});
