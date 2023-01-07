// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SettingsDialog', function() {
  test('Theme Selection', function() {
    const body = document.body;
    const queryString = 'input[type=\'radio\']:checked';
    chai.assert(body.classList.contains('light'));
    chai.assert.equal(document.querySelector(queryString).value, 'light');

    useTheme('dark');
    chai.assert(body.classList.contains('dark'));
    chai.assert.equal(document.querySelector(queryString).value, 'dark');

    useTheme('sepia');
    chai.assert(body.classList.contains('sepia'));
    chai.assert.equal(document.querySelector(queryString).value, 'sepia');
  });

  test('Font Family Selection', function() {
    const body = document.body;
    const fontFamilySelector = document.getElementById('font-family-selection');
    chai.assert.equal(
        fontFamilySelector[fontFamilySelector.selectedIndex].value,
        'sans-serif');
    chai.assert(body.classList.contains('sans-serif'));

    useFontFamily('serif');
    chai.assert.equal(
        fontFamilySelector[fontFamilySelector.selectedIndex].value, 'serif');
    chai.assert(body.classList.contains('serif'));

    useFontFamily('monospace');
    chai.assert.equal(
        fontFamilySelector[fontFamilySelector.selectedIndex].value,
        'monospace');
    chai.assert(body.classList.contains('monospace'));
  });
});
