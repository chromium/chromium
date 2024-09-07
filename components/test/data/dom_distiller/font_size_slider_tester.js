// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('FontSizeSlider', function() {
  test('Font Scale Desktop Selection', async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./chai.js');

    assert.strictEqual(pincher, undefined);
    const documentElement = document.documentElement;
    const fontSizeSelector = document.getElementById('font-size-selection');

    useFontScaling(1);
    assert.equal(documentElement.style.fontSize, '16px');

    useFontScaling(3);
    assert.equal(documentElement.style.fontSize, '48px');

    useFontScaling(0.875);
    assert.equal(documentElement.style.fontSize, '14px');
  });
});
