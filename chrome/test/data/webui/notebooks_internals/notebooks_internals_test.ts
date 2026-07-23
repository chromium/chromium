// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('NotebooksInternalsTest', () => {
  test('has expected page title', () => {
    assertEquals('notebooks internals', document.title.toLocaleLowerCase());
  });
});
