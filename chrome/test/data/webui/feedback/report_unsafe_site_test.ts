// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/report_unsafe_site/report_unsafe_site_app.js';

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ReportUnsafeSiteTest', () => {
  test('LoadWithoutErrors', () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);
    const cancelButton =
        app.shadowRoot.querySelector<HTMLInputElement>('.cancel-button');
    assertTrue(!!cancelButton);
    const okButton =
        app.shadowRoot.querySelector<HTMLInputElement>('.action-button');
    assertTrue(!!okButton);
  });
});
