// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webnn-internals/app.js';

import type {WebnnInternalsAppElement} from 'chrome://webnn-internals/app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('WebnnInternalsUITest', function() {
  let app: WebnnInternalsAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('webnn-internals-app');
    document.body.appendChild(app);
  });

  test('PageLoaded', function() {
    const tabElement = app.shadowRoot.querySelectorAll('cr-tabs');
    assertEquals(1, tabElement.length);
  });
});
