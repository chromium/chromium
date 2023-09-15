// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('HistoryClustersInternalsTest', () => {
  test('InternalsPageFeatureDisabled', () => {
    return new Promise(resolve => {
      setInterval(() => {
        const container = document.getElementById('log-message-container');
        assertTrue(!!container);
        if (container.children[0]!.childElementCount <= 2) {
          resolve(true);
        }
      }, 500);
    });
  });

  test('InternalsPageFeatureEnabled', () => {
    return new Promise(resolve => {
      setInterval(() => {
        const container = document.getElementById('log-message-container');
        assertTrue(!!container);
        if (container.children[0]!.childElementCount > 3) {
          resolve(true);
        }
      }, 500);
    });
  });
});
