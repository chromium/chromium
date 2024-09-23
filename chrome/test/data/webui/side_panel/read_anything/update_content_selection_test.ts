// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {suppressInnocuousErrors} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateContentSelection', () => {
  let app: AppElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;

  // root htmlTag='#document' id=1
  // ++paragraph htmlTag='p' id=2
  // ++++staticText name='Hello' id=3
  // ++paragraph htmlTag='p' id=4
  // ++++staticText name='World' id=5
  // ++paragraph htmlTag='p' id=6
  // ++++staticText name='Friend' id=7
  // ++++staticText name='!' id=8
  // ++++link htmlTag='a' id=9
  // +++++staticText name='You've Got a Friend in Me' id=10
  const inlineId = 10;
  const inlineText = 'You\'ve Got a Friend in Me';
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
        childIds: [7, 8, 9],
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
      {
        id: 9,
        role: 'link',
        htmlTag: 'a',
        display: 'inline',
        childIds: [inlineId],
      },
      {
        id: inlineId,
        role: 'staticText',
        name: inlineText,
      },
    ],
  };

  async function setSelection(selection: Object, contentNodes: number[]) {
    const selectedTree = Object.assign({selection: selection}, axTree);
    chrome.readingMode.setContentForTesting(selectedTree, contentNodes);
    return microtasksFinished();
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
  });

  test('forward selection inside distilled content', async () => {
    const expected = '<div><p>Hello</p><p>World</p><p>Friend!' +
        '<a>You\'ve Got a Friend in Me</a></p></div>';
    await setSelection(
        {
          anchor_object_id: 5,
          focus_object_id: 7,
          anchor_offset: 1,
          focus_offset: 2,
          is_backward: false,
        },
        [2, 4, 6]);

    const selection = app.getSelection();

    assertEquals(expected, app.$.container.innerHTML);
    assertEquals('World', selection.anchorNode.textContent);
    assertEquals('Friend', selection.focusNode.textContent);
    assertEquals(1, selection.anchorOffset);
    assertEquals(2, selection.focusOffset);
  });

  test('selection completely outside distilled content', async () => {
    const expected = '<div><p>World</p><p>Friend!' +
        '<a>You\'ve Got a Friend in Me</a></p></div>';
    await setSelection(
        {
          anchor_object_id: 5,
          focus_object_id: 7,
          anchor_offset: 1,
          focus_offset: 2,
          is_backward: false,
        },
        /* contentNodes= */[]);

    const selection = app.getSelection();

    assertEquals(expected, app.$.container.innerHTML);
    assertEquals('World', selection.anchorNode.textContent);
    assertEquals('Friend', selection.focusNode.textContent);
    assertEquals(1, selection.anchorOffset);
    assertEquals(2, selection.focusOffset);
  });

  test('selection partially outside distilled content', async () => {
    const expected = '<div><p>Hello</p><p>World</p><p>Friend!' +
        '<a>You\'ve Got a Friend in Me</a></p></div>';
    await setSelection(
        {
          anchor_object_id: 3,
          focus_object_id: 7,
          anchor_offset: 1,
          focus_offset: 2,
          is_backward: false,
        },
        [2, 4]);

    const selection = app.getSelection();

    assertEquals(expected, app.$.container.innerHTML);
    assertEquals('Hello', selection.anchorNode.textContent);
    assertEquals('Friend', selection.focusNode.textContent);
    assertEquals(1, selection.anchorOffset);
    assertEquals(2, selection.focusOffset);
  });

  test('backward selection inside distilled content', async () => {
    const expected = '<div><p>Hello</p><p>World</p><p>Friend!' +
        '<a>You\'ve Got a Friend in Me</a></p></div>';
    await setSelection(
        {
          anchor_object_id: 7,
          focus_object_id: 3,
          anchor_offset: 2,
          focus_offset: 1,
          is_backward: true,
        },
        [2, 4, 6]);

    const selection = app.getSelection();

    assertEquals(expected, app.$.container.innerHTML);
    assertEquals('Hello', selection.anchorNode.textContent);
    assertEquals('Friend', selection.focusNode.textContent);
    assertEquals(1, selection.anchorOffset);
    assertEquals(2, selection.focusOffset);
  });

  test('forward selection with inline text', async () => {
    await setSelection(
        {
          anchor_object_id: inlineId,
          focus_object_id: inlineId,
          anchor_offset: 3,
          focus_offset: 10,
          is_backward: false,
        },
        [2, 4, 6]);

    const selection = app.getSelection();

    assertEquals(inlineText, selection.anchorNode.textContent);
    assertEquals(inlineText, selection.focusNode.textContent);
    assertEquals(3, selection.anchorOffset);
    assertEquals(10, selection.focusOffset);
  });

  test('backward selection with inline text', async () => {
    await setSelection(
        {
          anchor_object_id: inlineId,
          focus_object_id: inlineId,
          anchor_offset: 10,
          focus_offset: 3,
          is_backward: true,
        },
        [2, 4, 6]);

    const selection = app.getSelection();

    assertEquals(inlineText, selection.anchorNode.textContent);
    assertEquals(inlineText, selection.focusNode.textContent);
    assertEquals(3, selection.anchorOffset);
    assertEquals(10, selection.focusOffset);
  });
});
