// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {FakeTree} from './fake_tree_builder.js';
import {FakeTreeBuilder} from './fake_tree_builder.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateContentSelection', () => {
  let app: AppElement;
  let fakeTree: FakeTree;

  const inlineId = 10;
  const inlineText = 'You\'ve Got a Friend in Me';

  async function setSelection(
      anchorId: number, anchorOffset: number, focusId: number,
      focusOffset: number) {
    fakeTree.setSelection(anchorId, anchorOffset, focusId, focusOffset);
    app.updateSelection();
    return microtasksFinished();
  }

  setup(() => {
    suppressInnocuousErrors();
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);

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
    fakeTree = new FakeTreeBuilder()
                   .root(1)
                   .addTag(2, /* parentId= */ 1, 'p')
                   .addText(3, /* parentId= */ 2, 'Hello')
                   .addTag(4, /* parentId= */ 1, 'p')
                   .addText(5, /* parentId= */ 4, 'World')
                   .addTag(6, /* parentId= */ 1, 'p')
                   .addText(7, /* parentId= */ 6, 'Friend')
                   .addText(8, /* parentId= */ 6, '!')
                   .addTag(9, /* parentId= */ 6, 'a')
                   .addText(inlineId, /* parentId= */ 9, inlineText)
                   .build(readingMode);
    app.updateContent();
  });

  test('selection of one text node', async () => {
    await setSelection(3, 0, 3, 4);

    const selection = app.getSelection();

    assertEquals(1, selection.rangeCount);
    assertEquals('Hello', selection.anchorNode.textContent);
    assertEquals('Hello', selection.focusNode.textContent);
    assertEquals(0, selection.anchorOffset);
    assertEquals(4, selection.focusOffset);
  });

  test('selection of multiple text nodes', async () => {
    await setSelection(5, 1, 7, 2);

    const selection = app.getSelection();

    assertEquals(1, selection.rangeCount);
    assertEquals('World', selection.anchorNode.textContent);
    assertEquals('Friend', selection.focusNode.textContent);
    assertEquals(1, selection.anchorOffset);
    assertEquals(2, selection.focusOffset);
  });

  test('selection of inline text', async () => {
    await setSelection(inlineId, 3, inlineId, 10);

    const selection = app.getSelection();

    assertEquals(1, selection.rangeCount);
    assertEquals(inlineText, selection.anchorNode.textContent);
    assertEquals(inlineText, selection.focusNode.textContent);
    assertEquals(3, selection.anchorOffset);
    assertEquals(10, selection.focusOffset);
  });

  test('selection of one parent node', async () => {
    await setSelection(4, 1, 4, 3);

    const selection = app.getSelection();

    assertEquals(1, selection.rangeCount);
    assertEquals('World', selection.anchorNode.textContent);
    assertEquals('World', selection.focusNode.textContent);
    assertEquals(1, selection.anchorOffset);
    assertEquals(3, selection.focusOffset);
  });

  test('selection of multple parent nodes', async () => {
    await setSelection(6, 3, 9, 10);

    const selection = app.getSelection();

    assertEquals(1, selection.rangeCount);
    assertEquals('Friend', selection.anchorNode.textContent);
    assertEquals(inlineText, selection.focusNode.textContent);
    assertEquals(3, selection.anchorOffset);
    assertEquals(10, selection.focusOffset);
  });

  test('invalid selection clears selection', async () => {
    chrome.readingMode.startNodeId = -1;
    chrome.readingMode.endNodeId = -1;

    const selection = app.getSelection();

    assertEquals(0, selection.rangeCount);
  });
});
