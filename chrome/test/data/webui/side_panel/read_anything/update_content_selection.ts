// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateContentSelection', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;

  // root htmlTag='#document' id=1
  // ++paragraph htmlTag='p' id=2
  // ++++staticText name='Hello' id=3
  // ++paragraph htmlTag='p' id=4
  // ++++staticText name='World' id=5
  // ++paragraph htmlTag='p' id=6
  // ++++staticText name='Friend' id=7
  // ++++staticText name='!' id=8
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4, 6],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'Hello',
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'World',
      },
      {
        id: 6,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [7, 8],
      },
      {
        id: 7,
        role: 'staticText',
        name: 'Friend',
      },
      {
        id: 8,
        role: 'staticText',
        name: '!',
      },
    ],
    selection: {
      anchor_object_id: 5,
      focus_object_id: 7,
      anchor_offset: 1,
      focus_offset: 2,
      is_backward: false,
    },
  };

  /**
   * Suppresses harmless ResizeObserver errors due to a browser bug.
   * yaqs/2300708289911980032
   */
  function suppressInnocuousErrors() {
    const onerror = window.onerror;
    window.onerror = (message, url, lineNumber, column, error) => {
      if ([
            'ResizeObserver loop limit exceeded',
            'ResizeObserver loop completed with undelivered notifications.',
          ].includes(message.toString())) {
        console.info('Suppressed ResizeObserver error: ', message);
        return;
      }
      if (onerror) {
        onerror.apply(window, [message, url, lineNumber, column, error]);
      }
    };
  }

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
    chrome.readingMode.setContentForTesting(axTree, []);
  });

  suite('WithSelection', () => {
    test('InnerHtmlCorrect', () => {
      const expected = '<div><p>World</p><p>Friend!</p></div>';
      const innerHTML = app.$.container.innerHTML;
      assertEquals(innerHTML, expected);
    });

    test('CorrectContentSelected', () => {
      // Calling shadowRoot.getSelection directly is not supported in TS tests,
      // so use a helper to get the selection from the app instead.
      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, 'World');
      assertEquals(selection.focusNode.textContent, 'Friend');
      assertEquals(selection.anchorOffset, 1);
      assertEquals(selection.focusOffset, 2);
    });
  });
});
