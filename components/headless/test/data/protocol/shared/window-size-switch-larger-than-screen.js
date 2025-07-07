// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}
// META: --window-size=1600,1200
//
(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank('Tests --window-size larger than screen.');

  const {result: {value}} =
      (await dp.Runtime.evaluate({
        expression: `window.outerWidth + 'x' + window.outerHeight`,
      })).result;

  testRunner.log('Window size: ' + value);

  testRunner.completeTest();
});
