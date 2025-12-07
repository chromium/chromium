// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('Sub feature policy test', () => {
  test('caption feature should be enabled', () => {
    assertTrue((window as any).loadTimeData!.getBoolean('captionEnabled'));
  });

  test('view screen feature should be enabled', () => {
    assertTrue((window as any).loadTimeData!.getBoolean('spotlightEnabled'));
  });
});
