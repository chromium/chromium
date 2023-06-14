// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

suite('LocationInternalsUITest', function() {
  test('PageLoaded', async function() {
    const watchButton = document.querySelector<HTMLElement>('#watch-btn');
    assert(watchButton);
  });
});
