// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';

suite('UpdateContentIntegration', () => {
  let app: AppElement;

  function setTree(rootChildren: number[], nodes: Object[]) {
    const tree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: rootChildren,
        },
        ...nodes,
      ],
    };

    chrome.readingMode.setContentForTesting(tree, rootChildren);
  }

  function assertHtml(expected: string) {
    assertEquals(expected, app.$.container.innerHTML);
  }

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chrome.readingMode.onConnected = () => {};

    // Override chrome.readingMode.requestImageData to avoid the cross-process
    // hop.
    chrome.readingMode.requestImageData = (nodeId: number) => {
      chrome.readingMode.onImageDownloaded(nodeId);
    };

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
  });

  test('image', () => {
    chrome.readingMode.getImageBitmap = (_: number) => {
      return {
        data: new Uint8ClampedArray(),
        width: 30,
        height: 40,
        scale: 0.5,
      };
    };

    const nodes = [
      {
        id: 2,
        role: 'image',
        htmlTag: 'img',
      },
    ];
    const expected = '<div><canvas alt="" class="downloaded-image" width="30"' +
        ' height="40" style="zoom: 0.5;"></canvas></div>';

    setTree([2], nodes);

    assertHtml(expected);
  });
});
