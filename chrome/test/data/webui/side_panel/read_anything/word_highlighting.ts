// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('WordHighlighting', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;

  // root htmlTag='#document' id=1
  // ++link htmlTag='a' url='http://www.google.com' id=2
  // ++++staticText name='This is a link.' id=3
  // ++link htmlTag='a' url='http://www.youtube.com' id=4
  // ++++staticText name='This is another link.' id=5
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.google.com',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is a link.',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.youtube.com',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is another link.',
      },
    ],
  };

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    flush();
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  // TODO(b/301131238): Before enabling the feature flag, ensure we've
  // added more robust tests.
  suite('with word boundary flag enabled after a word boundary', () => {
    setup(() => {
      app.updateBoundary(10);
    });

    test('word highlight used', () => {
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);

      // Sometimes the word returned can be "link", "link.", or "link. " which
      // can create flaky tests. Therefore, just check that the highlighted
      // text starts with "link" and isn't longer than the string would be if it
      // were "link. "
      // TODO(b/301131238): Investigate why there's a discrepancy here.
      assertTrue(currentHighlight!.textContent!.startsWith('link'));
      assertTrue(currentHighlight!.textContent!.length < 6);
    });

    test('with rate over 1 sentence highlight used', () => {
      app.rate = 2;
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent, 'This is a link.');
    });
  });

  suite('with word boundary flag with no word boundary', () => {
    setup(() => {
      app.playSpeech();
    });

    test('sentence highlight used', () => {
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent, 'This is a link.');
    });
  });
});
