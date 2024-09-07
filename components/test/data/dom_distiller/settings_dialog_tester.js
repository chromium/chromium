// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SettingsDialog', function() {
  test('Theme Selection', async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./chai.js');

    const body = document.body;
    const queryString = 'input[type=\'radio\']:checked';
    assert.isTrue(body.classList.contains('light'));
    assert.equal(document.querySelector(queryString).value, 'light');

    useTheme('dark');
    assert.isTrue(body.classList.contains('dark'));
    assert.equal(document.querySelector(queryString).value, 'dark');

    useTheme('sepia');
    assert.isTrue(body.classList.contains('sepia'));
    assert.equal(document.querySelector(queryString).value, 'sepia');
  });

  test('Font Family Selection', async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./chai.js');

    const body = document.body;
    const fontFamilySelector = document.getElementById('font-family-selection');
    assert.equal(
        fontFamilySelector[fontFamilySelector.selectedIndex].value,
        'sans-serif');
    assert.isTrue(body.classList.contains('sans-serif'));

    useFontFamily('serif');
    assert.equal(
        fontFamilySelector[fontFamilySelector.selectedIndex].value, 'serif');
    assert.isTrue(body.classList.contains('serif'));

    useFontFamily('monospace');
    assert.equal(
        fontFamilySelector[fontFamilySelector.selectedIndex].value,
        'monospace');
    assert.isTrue(body.classList.contains('monospace'));
  });
});
