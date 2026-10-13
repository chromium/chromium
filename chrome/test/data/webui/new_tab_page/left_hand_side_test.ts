// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <cr-left-hand-side> renders an <ntp-iframe>, which is defined in the lazily
// loaded part of the NTP.
import 'chrome://new-tab-page/lazy_load.js';

import type {IframeElement, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import type {LeftHandSideElement} from 'chrome://new-tab-page/new_tab_page.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('NewTabPageLeftHandSideTest', () => {
  let leftHandSide: LeftHandSideElement;
  let windowProxy: TestMock<WindowProxy>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    // Prevents the iframe from actually loading the LHS page.
    windowProxy.setResultFor('createIframeSrc', '');
  });

  async function createLeftHandSide() {
    leftHandSide = document.createElement('cr-left-hand-side');
    document.body.appendChild(leftHandSide);
    await microtasksFinished();
  }

  test('renders folded rail when hybrid is enabled', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: true});
    await createLeftHandSide();

    const folded =
        leftHandSide.shadowRoot.querySelector<ThreadsRailElement>('#folded');
    assertTrue(!!folded);
    assertEquals('CR-THREADS-RAIL', folded.tagName);
  });

  test('does not render folded rail when hybrid is disabled', async () => {
    loadTimeData.overrideValues({enableThreadsRailHybrid: false});
    await createLeftHandSide();

    assertFalse(!!leftHandSide.shadowRoot.querySelector('#folded'));
  });

  [true, false].forEach(enableThreadsRailHybrid => {
    const mode = enableThreadsRailHybrid ? 'hybrid' : 'non-hybrid';

    test(`renders expanded LHS iframe in ${mode} mode`, async () => {
      loadTimeData.overrideValues({enableThreadsRailHybrid});
      await createLeftHandSide();

      const iframe =
          leftHandSide.shadowRoot.querySelector<IframeElement>('#expanded');
      assertTrue(!!iframe);
      assertEquals('NTP-IFRAME', iframe.tagName);
      assertEquals('chrome-untrusted://new-tab-page/expanded-lhs', iframe.src);
    });
  });
});
