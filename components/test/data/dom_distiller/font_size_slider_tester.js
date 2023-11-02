// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('FontSizeSlider', function() {
  test('Font Scale Desktop Selection', function() {
    chai.assert.strictEqual(pincher, undefined);
    const documentElement = document.documentElement;
    const fontSizeSelector = document.getElementById('font-size-selection');

    useFontScaling(1);
    chai.assert.equal(documentElement.style.fontSize, '16px');

    useFontScaling(3);
    chai.assert.equal(documentElement.style.fontSize, '48px');

    useFontScaling(0.875);
    chai.assert.equal(documentElement.style.fontSize, '14px');
  });
});
