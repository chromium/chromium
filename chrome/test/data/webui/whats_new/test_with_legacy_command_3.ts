// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);
  window.top.postMessage(
      {'data': {'commandId': 3, 'clickInfo': {}}}, 'chrome://whats-new/');
};
