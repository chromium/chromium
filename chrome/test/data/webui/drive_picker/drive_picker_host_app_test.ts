// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://drive-picker-host/app.js';

import type {DrivePickerHostAppElement} from 'chrome://drive-picker-host/app.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('DrivePickerHostAppTest', function() {
  let app: DrivePickerHostAppElement;

  setup(async function() {
    if (window.trustedTypes) {
      document.body.innerHTML = window.trustedTypes.emptyHTML;
    } else {
      document.body.innerHTML = '';
    }
    app = document.createElement('drive-picker-host-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('AppIsAttached', function() {
    assertTrue(app.isConnected);
  });

  test('HostsUntrustedIframe', function() {
    assertTrue(!!app.shadowRoot);
    const iframe = app.shadowRoot.querySelector('iframe');
    assertTrue(!!iframe);
    assertEquals('chrome-untrusted://drive-picker-host/', iframe.src);
  });
});
