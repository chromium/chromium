// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('CommerceInternalsTest', () => {
  test('page title', () => {
    assertEquals('commerce internals', document.title.toLocaleLowerCase());
  });
});
