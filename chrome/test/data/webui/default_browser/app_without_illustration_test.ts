// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://default-browser-modal/app.js';

import type {DefaultBrowserModalAppElement} from 'chrome://default-browser-modal/app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('DefaultBrowserModalAppWithoutIllustrationTest', function() {
  let app: DefaultBrowserModalAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({useSettingsIllustration: false});
    app = document.createElement('default-browser-modal-app');
    document.body.appendChild(app);
  });

  test('CheckLayout', function() {
    assertFalse(app.useSettingsIllustration);

    const iconContainer = app.shadowRoot.querySelector('#icon-container');
    assertTrue(isVisible(iconContainer));

    const headerBackground = app.shadowRoot.querySelector('#header-background');
    assertTrue(isVisible(headerBackground));

    const icon = app.shadowRoot.querySelector('#icon');
    assertTrue(isVisible(icon));

    const illustration = app.shadowRoot.querySelector('#illustration');
    assertFalse(isVisible(illustration));
  });
});
