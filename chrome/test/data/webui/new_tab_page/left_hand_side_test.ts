// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <cr-left-hand-side> renders an <ntp-iframe>, which is defined in the lazily
// loaded part of the NTP.
import 'chrome://new-tab-page/lazy_load.js';

import type {IframeElement, ThreadsRailElement} from 'chrome://new-tab-page/lazy_load.js';
import type {LeftHandSideElement} from 'chrome://new-tab-page/new_tab_page.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('NewTabPageLeftHandSideTest', () => {
  let leftHandSide: LeftHandSideElement;
  let windowProxy: TestMock<WindowProxy>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    // Prevents the iframe from actually loading the LHS page.
    windowProxy.setResultFor('createIframeSrc', '');

    leftHandSide = document.createElement('cr-left-hand-side');
    document.body.appendChild(leftHandSide);
    await microtasksFinished();
  });

  test('renders folded threads rail', () => {
    const folded =
        leftHandSide.shadowRoot.querySelector<ThreadsRailElement>('#folded');
    assertTrue(!!folded);
    assertEquals('CR-THREADS-RAIL', folded.tagName);
  });

  test('renders expanded LHS iframe', () => {
    const iframe =
        leftHandSide.shadowRoot.querySelector<IframeElement>('#expanded');
    assertTrue(!!iframe);
    assertEquals('NTP-IFRAME', iframe.tagName);
    assertEquals('chrome-untrusted://new-tab-page/expanded-lhs', iframe.src);
  });
});
