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
        for (let i = 0; i < mutation.addedNodes.length; i++) {
          // Here we can inspect each of the added nodes. We expect
          // to find one that contains one of the GPU status strings.
          const addedNode = mutation.addedNodes[i];
          // Check for both. If it contains neither, it's an unrelated
          // mutation event we don't care about. But if it contains one,
          // pass or fail accordingly.
          const gpuyes = addedNode.innerText.match(gpuyesstring);
          const gpuno = addedNode.innerText.match(gpunostring);
          if (gpuyes || gpuno) {
            assertEquals(null, gpuno);
            assertTrue(gpuyes && (gpuyes[0] === gpuyesstring));
            done();
          }
        }
      });
    });
    observer.observe(document.getElementById('basic-info'), {childList: true});
  });
});
