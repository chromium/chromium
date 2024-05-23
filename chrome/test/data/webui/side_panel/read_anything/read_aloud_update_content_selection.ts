// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {PauseActionSource} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors, waitForPlayFromSelection} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('ReadAloud_UpdateContentSelection', () => {
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
    document.onselectionchange = () => {};
    chrome.readingMode.setContentForTesting(axTree, []);
  });

  suite('Before speech started', () => {
    test('inner html of container matches expected html', () => {
      assertTrue(app.speechPlayingState.paused);
      assertFalse(app.speechPlayingState.speechStarted);
      // The expected HTML before any highlights are added.
      const expected = '<div><p>World</p><p>Friend!</p></div>';
      const innerHTML = app.$.container.innerHTML;
      assertEquals(innerHTML, expected);
    });

    test('selection in reading mode panel correct', () => {
      // Calling shadowRoot.getSelection directly is not supported in TS tests,
      // so use a helper to get the selection from the app instead.
      const selection = document.getSelection()!;
      assertTrue(selection != null);
      assertEquals(selection.anchorNode!.textContent, 'World');
      assertEquals(selection.focusNode!.textContent, 'Friend');
      assertEquals(selection.anchorOffset, 1);
      assertEquals(selection.focusOffset, 2);
    });

    test('conbtainer class correct', () => {
      assertEquals(
          app.$.container.className,
          'user-select-disabled-when-speech-playing-false');
      assertEquals(window.getComputedStyle(app.$.container).userSelect, 'auto');
    });
  });

  suite('While Read Aloud playing', () => {
    setup(async () => {
      app.playSpeech();
      await waitForPlayFromSelection();
    });

    test('inner html of container matches expected html', () => {
      assertFalse(app.speechPlayingState.paused);
      assertTrue(app.speechPlayingState.speechStarted);
      // The expected HTML with the current highlights.
      const expected = '<div><p><span class="parent-of-highlight">' +
          '<span class="current-read-highlight">World</span>' +
          '</span></p><p><span class="parent-of-highlight">' +
          '<span class="current-read-highlight">Friend' +
          '</span></span><span class="parent-of-highlight">' +
          '<span class="current-read-highlight">!</span>' +
          '</span></p></div>';
      const innerHTML = app.$.container.innerHTML;
      assertEquals(innerHTML, expected);
    });

    test('selection in reading mode panel cleared', () => {
      const selection = document.getSelection()!;
      assertEquals(selection.toString(), '');
    });

    test('container class correct', () => {
      assertEquals(
          app.$.container.className,
          'user-select-disabled-when-speech-playing-true');
      assertEquals(window.getComputedStyle(app.$.container).userSelect, 'none');
    });
  });

  suite('While Read Aloud paused', () => {
    setup(() => {
      app.playSpeech();
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
    });
    test('inner html of container matches expected html', () => {
      assertTrue(app.speechPlayingState.paused);
      assertTrue(app.speechPlayingState.speechStarted);
      // The expected HTML with the current highlights.
      const expected = '<div><p><span class="parent-of-highlight">' +
          '<span class="current-read-highlight">World</span>' +
          '</span></p><p><span class="parent-of-highlight">' +
          '<span class="current-read-highlight">Friend' +
          '</span></span><span class="parent-of-highlight">' +
          '<span class="current-read-highlight">!</span>' +
          '</span></p></div>';
      const innerHTML = app.$.container.innerHTML;
      assertEquals(innerHTML, expected);
    });

    test('selection in reading mode panel cleared', () => {
      const selection = document.getSelection()!;
      assertEquals(selection.toString(), '');
    });

    test('container class correct', () => {
      assertEquals(
          app.$.container.className,
          'user-select-disabled-when-speech-playing-false');
      assertEquals(window.getComputedStyle(app.$.container).userSelect, 'auto');
    });
  });
});
