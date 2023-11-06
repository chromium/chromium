// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('GPU', () => {
  test('GPUSandboxEnabled', (done) => {
    const gpuyesstring = 'Sandboxed\ttrue';
    const gpunostring = 'Sandboxed\tfalse';

    const observer = new MutationObserver(function(mutations) {
      mutations.forEach(function(mutation) {
        for (const addedNode of mutation.addedNodes as NodeListOf<HTMLElement>) {
          // Here we can inspect each of the added nodes. We expect
          // to find one that contains one of the GPU status strings.
          // Check for both. If it contains neither, it's an unrelated
          // mutation event we don't care about. But if it contains one,
          // pass or fail accordingly.
          const gpuyes = addedNode.innerText.match(gpuyesstring);
          const gpuno = addedNode.innerText.match(gpunostring);
          if (gpuno !== null || gpuyes !== null) {
            assertEquals(null, gpuno);
            assertTrue(!!gpuyes && gpuyes[0] === gpuyesstring);
            done();
          }
        }
      });
    });
    const basicInfo = document.getElementById('basic-info');
    assertTrue(!!basicInfo);
    observer.observe(basicInfo, {childList: true});
  });
});
