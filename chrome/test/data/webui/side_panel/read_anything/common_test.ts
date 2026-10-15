// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getWordCount, isBlockTrackingEnabled} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setupTestEnvironment} from './common.js';
import type {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('Common', () => {
  test('getWordCount', () => {
    assertEquals(5, getWordCount('Nothing but the truth now'));
    assertEquals(8, getWordCount('Nothing but the\nproof of\n what I am'));
    assertEquals(1, getWordCount('TheworstofwhatIcamefrom'));
    assertEquals(
        4, getWordCount('patterns           I\'m ashamed\n\n\n\n\n\n   of'));
  });

  suite('isBlockTrackingEnabled', () => {
    let contentBrowserProxy: TestContentBrowserProxy;
    let visualBrowserProxy: TestVisualBrowserProxy;

    setup(() => {
      ({contentBrowserProxy, visualBrowserProxy} = setupTestEnvironment());
    });

    test('true if TranslatePdf is enabled and the page is a PDF', () => {
      contentBrowserProxy.translatePdfEnabled = true;
      visualBrowserProxy.pdf = true;
      assertTrue(isBlockTrackingEnabled());
    });

    test('false if TranslatePdf is disabled', () => {
      contentBrowserProxy.translatePdfEnabled = false;
      visualBrowserProxy.pdf = true;
      assertFalse(isBlockTrackingEnabled());
    });

    test('false if the page is not a PDF', () => {
      contentBrowserProxy.translatePdfEnabled = true;
      visualBrowserProxy.pdf = false;
      assertFalse(isBlockTrackingEnabled());
    });
  });
});
