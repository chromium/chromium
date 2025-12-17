// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://device-log/app.js';

import type {DeviceLogAppElement} from 'chrome://device-log/app.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('DeviceLog', function() {
  let app: DeviceLogAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('device-log-app');
    document.body.appendChild(app);
  });

  test('element exists', function() {
    // TODO(crbug.com/469125041): add tests.
    assertTrue(!!app);
  });
});
