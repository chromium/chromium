// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 \
// META:   workAreaLeft=10 workAreaRight=90 workAreaTop=20 workAreaBottom=80}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank('Tests window.screen.avail* APIs.');

  const expression = `
      let lines = [];
      lines.push('availLeft=' + window.screen.availLeft);
      lines.push('availTop=' + window.screen.availTop);
      lines.push('availWidth=' + window.screen.availWidth);
      lines.push('availHeight=' + window.screen.availHeight);
      lines.join(' ');
    `;

  const {result} = (await dp.Runtime.evaluate({expression})).result;

  testRunner.log(result.value);

  testRunner.completeTest();
});
