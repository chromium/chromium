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

  test('PostProcessElementCatchesExceptions', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    document.body.appendChild(container);

    // Save original functions.
    const originalRemove = window.removeExtraneousElementsFrom;
    const originalConsoleError = console.error;

    let errorLogged = false;

    // Mock functions.
    window.removeExtraneousElementsFrom = () => {
      throw new Error('Test Error');
    };

    console.error = (msg) => {
      if (msg === 'Post-processing step failed.') {
        errorLogged = true;
      }
    };

    try {
      postProcessElement(container);
    } finally {
      // Restore original functions.
      window.removeExtraneousElementsFrom = originalRemove;
      console.error = originalConsoleError;
    }

    assert.isTrue(errorLogged, 'Error should be logged to console');
  });

  test('PostProcessElementContinuesOnFailure', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    document.body.appendChild(container);

    // Save original functions.
    const originalRemove = window.removeExtraneousElementsFrom;
    const originalWrap = window.wrapTables;
    const originalConsoleError = console.error;

    let wrapCalled = false;

    // Mock functions.
    window.removeExtraneousElementsFrom = () => {
      throw new Error('Test Error');
    };
    window.wrapTables = () => {
      wrapCalled = true;
    };
    // Suppress console.error during this test to avoid polluting output.
    console.error = () => {};

    try {
      postProcessElement(container);
    } finally {
      // Restore original functions.
      window.removeExtraneousElementsFrom = originalRemove;
      window.wrapTables = originalWrap;
      console.error = originalConsoleError;
    }

    assert.isTrue(wrapCalled, 'Subsequent steps should still run');
  });
});
