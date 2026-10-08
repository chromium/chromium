// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tabstrip-ntb.top-chrome/app.js';

import type {TabstripNtbAppElement} from 'chrome://tabstrip-ntb.top-chrome/app.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NewTabButtonTest', function() {
  let app: TabstripNtbAppElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('tabstrip-ntb-app');
    document.body.appendChild(app);
    await app.updateComplete;
  });

  test('ClicksNewTabButton', async function() {
    const newTabButton = app.shadowRoot.querySelector('new-tab-button');
    assertTrue(!!newTabButton);
    await newTabButton.updateComplete;
    const button =
        newTabButton.shadowRoot.querySelector<HTMLElement>('cr-icon-button');
    assertTrue(!!button);
    button.click();
  });
});
