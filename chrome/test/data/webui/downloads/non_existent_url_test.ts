// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('NonExistentUrl', function() {
  test('should load main page with no console errors', async function() {
    await customElements.whenDefined('downloads-manager');
    assertEquals('chrome://downloads/', location.href);
  });
});
