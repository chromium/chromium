// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/footer.js';

import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {FooterElement} from 'chrome://customize-chrome-side-panel.top-chrome/footer.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FooterTest', () => {
  let footer: FooterElement;
  let metrics: MetricsTracker;

  setup(() => {
    metrics = fakeMetricsPrivate();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    footer = document.createElement('customize-chrome-footer');
    document.body.appendChild(footer);
    return microtasksFinished();
  });

  test('clicking footer toggle sets metric', async () => {
    const toggle = footer.$.showToggle;
    toggle.click();
    await microtasksFinished();

    assertEquals(1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.CustomizeChromeSidePanelAction',
            CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED));
  });
});
